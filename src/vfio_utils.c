#include "vfio_utils.h"

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/eventfd.h>

int vfio_container_create(vfio_container_t* container) {
    container->n_groups = 0;
    container->fd = open("/dev/vfio/vfio", O_RDWR);
    if (ioctl(container->fd, VFIO_GET_API_VERSION) != VFIO_API_VERSION) return -1;
    if (!ioctl(container->fd, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU))  return -2;
    return 0;
}

int vfio_group_create(vfio_group_t* vfio_group, vfio_container_t* container, char* vfio_path) {
    vfio_group->status.argsz = sizeof(struct vfio_group_status);

    vfio_group->container = container;
    vfio_group->group = open(vfio_path, O_RDWR);

    struct vfio_group_status group_status = {
        .argsz = sizeof(group_status)
    };
    ioctl(vfio_group->group, VFIO_GROUP_GET_STATUS, &group_status);
    if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE))  return -3;  // Not all devices bound

    ioctl(vfio_group->group, VFIO_GROUP_SET_CONTAINER, &(container->fd));
    if (container->n_groups == 0) {
        ioctl(container->fd, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);
    }

    container->n_groups++;

    return 0;
}

int vfio_group_map_create(vfio_group_t* vfio_group, void* addr, uint64_t size, void* virtual_addr) {
    struct vfio_iommu_type1_dma_map dma_map = {
        .argsz = sizeof(dma_map),
        .vaddr = (uint64_t)addr,
        .size = size,
        .flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE,
        .iova = (uint64_t)virtual_addr
    };
    if (ioctl(vfio_group->container->fd, VFIO_IOMMU_MAP_DMA, &dma_map) < 0) return -1;
    return 0;
}

int vfio_device_region_get(vfio_device_t* device, int index) {
    struct vfio_region_info info = {
        .argsz = sizeof(info),
        .index = index
    };

    if (ioctl(device->fd, VFIO_DEVICE_GET_REGION_INFO, &info) < 0) return -1;

    device->regions[index].flags   = info.flags;
    device->regions[index].offset  = info.offset;
    device->regions[index].size    = info.size;

    return 0;
}

int vfio_device_add(vfio_device_t* device, vfio_group_t* vfio_group, char* dbdf) {

    device->info.argsz = sizeof(struct vfio_device_info);

    device->fd = ioctl(vfio_group->group, VFIO_GROUP_GET_DEVICE_FD, dbdf);
    if (device->fd < 0) return -2;

    if (ioctl(device->fd, VFIO_DEVICE_GET_INFO, &(device->info)) < 0) return -3;

    if (vfio_device_region_get(device, VFIO_PCI_CONFIG_REGION_INDEX) < 0) return -3;
    device->config_offset = device->regions[VFIO_PCI_CONFIG_REGION_INDEX].offset;

    return 0;
}

int vfio_device_region_attach(vfio_device_t* device, int region_idx) {
    vfio_region_t* region = &(device->regions[region_idx]);

    if (vfio_device_region_get(device, region_idx) < 0) return -1;

    int access_flags = 0;
    if (!(region->flags & VFIO_REGION_INFO_FLAG_MMAP)) return -2;  // Region does not support mmap
    if (region->flags & VFIO_REGION_INFO_FLAG_READ) {
        access_flags |= PROT_READ;
    }
    if (region->flags & VFIO_REGION_INFO_FLAG_WRITE) {
        access_flags |= PROT_WRITE;
    }
    region->mem = mmap(NULL, region->size, access_flags, MAP_SHARED, device->fd, region->offset);
    if (region->mem == NULL) return -3;  // mmap failed
    if ((uint64_t)(region->mem) == 0xffffffffffffffff) return -4;  // mmap failed
    return 0;
}

int vfio_device_config_read(vfio_device_t* device, uint8_t* data, uint64_t offset, uint64_t length) {
    ssize_t num_read = pread(device->fd, data, length, device->config_offset);
    if (num_read < length) return -1;
    return 0;
}

int vfio_device_config_write(vfio_device_t* device, uint8_t* data, uint64_t offset, uint64_t length) {
    ssize_t num_wrote = pwrite(device->fd, data, length, device->config_offset);
    if (num_wrote < length) return -1;
    return 0;
}

int vfio_device_reset(vfio_device_t* device) {
    if(ioctl(device->fd, VFIO_DEVICE_RESET) < 0) return -1;
    return 0;
}

int vfio_device_max_interrupts_get(vfio_device_t* device, uint32_t* count) {
    struct vfio_irq_info msix_irq_info = {
        .argsz = sizeof(msix_irq_info),
        .index = VFIO_PCI_MSIX_IRQ_INDEX
    };
    if (ioctl(device->fd, VFIO_DEVICE_GET_IRQ_INFO, &msix_irq_info) < 0) return -1;
    *count = msix_irq_info.count;
    return 0;
}

int vfio_device_interrupts_assign(vfio_device_t* device, uint32_t start, uint32_t count, int32_t* fds) {
    uint32_t buf_size = sizeof(struct vfio_irq_set) + (sizeof(int32_t) * count);
    struct vfio_irq_set* irqs = calloc(buf_size, 1);
    if (irqs == NULL) return -1;

    irqs->argsz = buf_size;
    irqs->start = start;
    irqs->count = count;
    irqs->flags = VFIO_IRQ_SET_DATA_EVENTFD | VFIO_IRQ_SET_ACTION_TRIGGER;
    irqs->index = VFIO_PCI_MSIX_IRQ_INDEX;

    int32_t* fd_ptr = (int32_t*)&irqs->data[0];
    uint32_t i;
    for (i = 0; i < count; i++) {
        fd_ptr[i] = fds[i];
    }

    if (ioctl(device->fd, VFIO_DEVICE_SET_IRQS, irqs) < 0) return -2;
    free(irqs);
    return 0;
}

int vfio_device_interrupts_clear(vfio_device_t* device, uint32_t start, uint32_t count) {
    int32_t* fds = calloc(sizeof(int32_t) * count, 1);
    if (fds == NULL) return -1;
    uint32_t i;
    for (i = 0; i < count; i++) {
        fds[i] = -1;
    }
    if (vfio_device_interrupts_assign(device, start, count, fds) < 0) return -2;
    return 0;
}

int vfio_device_interrupts_create(vfio_device_t* device, uint32_t start, uint32_t count, int32_t* fds) {
    if (fds == NULL) return -1;
    uint32_t i;
    for (i = 0; i < count; i++) {
        int32_t fd;
        fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        fds[i] = fd;
    }
    if (vfio_device_interrupts_assign(device, start, count, fds) < 0) return -2;
    return 0;
}

int vfio_device_vendor_id_read(vfio_device_t* device, uint16_t* vendor_id) {
    int ret = vfio_device_config_read(device, (uint8_t*)(vendor_id), device->config_offset, 2);
    if (ret < 0) return ret;
    return 0;
}

int vfio_device_device_id_read(vfio_device_t* device, uint16_t* device_id) {
    int ret = vfio_device_config_read(device, (uint8_t*)(device_id), device->config_offset + 2, 2);
    if (ret < 0) return ret;
    return 0;
}

int vfio_device_enable(vfio_device_t* device) {
    uint16_t command = 0x6;
    int ret = vfio_device_config_write(device, (uint8_t*)(&command), device->config_offset + 4, 2);
    if (ret < 0) return ret;
    return 0;
}
