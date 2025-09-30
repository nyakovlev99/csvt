#include "vfio_group.h"

#include <linux/vfio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/eventfd.h>

typedef struct {
    int container;
    int group;
    struct vfio_group_status status;
} vfio_group_t;

typedef struct {
    int fd;
    struct vfio_device_info info;
    uint64_t config_offset;
} device_t;

int vfio_group_create(char* vfio_path, void** vfio_group) {
    *vfio_group = calloc(sizeof(vfio_group_t), 1);
    if (*vfio_group == NULL) return -1;

    vfio_group_t* _vfio_group = (vfio_group_t*)*vfio_group;
    _vfio_group->status.argsz = sizeof(struct vfio_group_status);

    _vfio_group->container = open("/dev/vfio/vfio", O_RDWR);
    if (ioctl(_vfio_group->container, VFIO_GET_API_VERSION) != VFIO_API_VERSION) return -1;
    if (!ioctl(_vfio_group->container, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU))  return -2;
    _vfio_group->group = open(vfio_path, O_RDWR);

    struct vfio_group_status group_status = {
        .argsz = sizeof(group_status)
    };
    ioctl(_vfio_group->group, VFIO_GROUP_GET_STATUS, &group_status);
    if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE))  return -3;  // Not all devices bound

    ioctl(_vfio_group->group, VFIO_GROUP_SET_CONTAINER, &(_vfio_group->container));
    ioctl(_vfio_group->container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);

    return 0;
}

int vfio_group_destroy(void* vfio_group) {
    free(vfio_group);
    return 0;
}

int vfio_group_map_create(void* vfio_group, void* addr, uint64_t size, void* virtual_addr) {
    vfio_group_t* _vfio_group = (vfio_group_t*)vfio_group;
    struct vfio_iommu_type1_dma_map dma_map = {
        .argsz = sizeof(dma_map),
        .vaddr = (uint64_t)addr,
        .size = size,
        .flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE,
        .iova = (uint64_t)virtual_addr
    };
    if (ioctl(_vfio_group->container, VFIO_IOMMU_MAP_DMA, &dma_map) < 0) return -1;
    return 0;
}

int vfio_group_device_add(void* vfio_group, char* dbdf, void** device) {
    vfio_group_t* _vfio_group = (vfio_group_t*)vfio_group;

    *device = calloc(sizeof(device_t), 1);
    if (*device == NULL) return -1;

    device_t* _device = (device_t*)*device;
    _device->info.argsz = sizeof(struct vfio_device_info);

    _device->fd = ioctl(_vfio_group->group, VFIO_GROUP_GET_DEVICE_FD, dbdf);
    if (_device->fd < 0) return -2;

    if (ioctl(_device->fd, VFIO_DEVICE_GET_INFO, &(_device->info)) < 0) return -3;

    uint32_t config_flags;
    uint64_t config_size;
    if (vfio_group_device_region_get(
        *device,
        VFIO_PCI_CONFIG_REGION_INDEX,
        &config_flags,
        &(_device->config_offset),
        &config_size
    ) < 0) {
        return -3;
    }

    return 0;
}

int vfio_group_device_num_regions(void* device, uint32_t* num_regions) {
    device_t* _device = (device_t*)device;
    *num_regions = _device->info.num_regions;
    return 0;
}

int vfio_group_device_region_get(void* device, int index, uint32_t* flags, uint64_t* offset, uint64_t* size) {
    device_t* _device = (device_t*)device;

    struct vfio_region_info info = {
        .argsz = sizeof(info),
        .index = index
    };

    if (ioctl(_device->fd, VFIO_DEVICE_GET_REGION_INFO, &info) < 0) return -1;

    *flags = info.flags;
    *offset = info.offset;
    *size = info.size;

    return 0;
}

int vfio_group_device_region_attach(void* device, uint32_t flags, uint64_t offset, uint64_t size, void** region) {
    device_t* _device = (device_t*)device;
    int access_flags = 0;
    if (!(flags & VFIO_REGION_INFO_FLAG_MMAP)) return -1;  // Region does not support mmap
    if (flags & VFIO_REGION_INFO_FLAG_READ) {
        access_flags |= PROT_READ;
    }
    if (flags & VFIO_REGION_INFO_FLAG_WRITE) {
        access_flags |= PROT_WRITE;
    }
    *region = mmap(NULL, size, access_flags, MAP_SHARED, _device->fd, offset);
    if (*region == NULL) return -2;  // mmap failed
    if ((uint64_t)(*region) == 0xffffffffffffffff) return -3;  // mmap failed
    return 0;
}

int vfio_group_device_config_read(void* device, uint8_t* data, uint64_t offset, uint64_t length) {
    device_t* _device = (device_t*)device;
    ssize_t num_read = pread(_device->fd, data, length, _device->config_offset);
    if (num_read < length) return -1;
    return 0;
}

int vfio_group_device_config_write(void* device, uint8_t* data, uint64_t offset, uint64_t length) {
    device_t* _device = (device_t*)device;
    ssize_t num_wrote = pwrite(_device->fd, data, length, _device->config_offset);
    if (num_wrote < length) return -1;
    return 0;
}

void vfio_group_device_get_descriptor(void* device, int* fd) {
    device_t* _device = (device_t*)device;
    *fd = _device->fd;
}

void vfio_group_device_get_config_offset(void* device, uint64_t* offset) {
    device_t* _device = (device_t*)device;
    *offset = _device->config_offset;
}

int vfio_group_device_reset(void* device) {
    device_t* _device = (device_t*)device;
    if(ioctl(_device->fd, VFIO_DEVICE_RESET) < 0) return -1;
    return 0;
}

int vfio_group_device_max_interrupts_get(void* device, uint32_t* count) {
    device_t* _device = (device_t*)device;
    struct vfio_irq_info msix_irq_info = {
        .argsz = sizeof(msix_irq_info),
        .index = VFIO_PCI_MSIX_IRQ_INDEX
    };
    if (ioctl(_device->fd, VFIO_DEVICE_GET_IRQ_INFO, &msix_irq_info) < 0) return -1;
    *count = msix_irq_info.count;
    return 0;
}

int vfio_group_device_interrupts_assign(void* device, uint32_t start, uint32_t count, int32_t* fds) {
    device_t* _device = (device_t*)device;
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

    if (ioctl(_device->fd, VFIO_DEVICE_SET_IRQS, irqs) < 0) return -2;
    free(irqs);
    return 0;
}

int vfio_group_device_interrupts_clear(void* device, uint32_t start, uint32_t count) {
    int32_t* fds = calloc(sizeof(int32_t) * count, 1);
    if (fds == NULL) return -1;
    uint32_t i;
    for (i = 0; i < count; i++) {
        fds[i] = -1;
    }
    if (vfio_group_device_interrupts_assign(device, start, count, fds) < 0) return -2;
    return 0;
}

int vfio_group_device_interrupts_create(void* device, uint32_t start, uint32_t count, int32_t* fds) {
    if (fds == NULL) return -1;
    uint32_t i;
    for (i = 0; i < count; i++) {
        int32_t fd;
        fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        fds[i] = fd;
    }
    if (vfio_group_device_interrupts_assign(device, start, count, fds) < 0) return -2;
    return 0;
}
