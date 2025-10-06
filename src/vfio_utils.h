#ifndef __VFIO_UTILS_INCLUDE__
#define __VFIO_UTILS_INCLUDE__

#include <stdint.h>
#include <linux/vfio.h>

typedef struct {
    int fd;
    int n_groups;
} vfio_container_t;

typedef struct {
    vfio_container_t* container;
    int group;
    struct vfio_group_status status;
} vfio_group_t;

typedef struct {
    int         index;
    uint32_t    flags;
    uint64_t    offset;
    uint64_t    size;
    void*       mem;
} vfio_region_t;

typedef struct {
    int                     fd;
    struct vfio_device_info info;
    uint64_t                config_offset;
    vfio_region_t           regions[VFIO_PCI_NUM_REGIONS];
} vfio_device_t;

int vfio_init();
int vfio_container_create(vfio_container_t* container);
int vfio_group_create(vfio_group_t* vfio_group, vfio_container_t* container, char* vfio_path);
int vfio_group_map_create(vfio_group_t* vfio_group, void* addr, uint64_t size, uint64_t virtual_addr);
int vfio_device_add(vfio_device_t* device, vfio_group_t* vfio_group, char* dbdf);
int vfio_device_num_regions(vfio_device_t* device, uint32_t* num_regions);
int vfio_device_region_get(vfio_device_t* device, int index);
int vfio_device_region_attach(vfio_device_t* device, int region_idx);
int vfio_device_config_read(vfio_device_t* device, uint8_t* data, uint64_t offset, uint64_t length);
int vfio_device_config_write(vfio_device_t* device, uint8_t* data, uint64_t offset, uint64_t length);
void vfio_device_get_descriptor(vfio_device_t* device, int* fd);
void vfio_device_get_config_offset(vfio_device_t* device, uint64_t* offset);
int vfio_device_reset(vfio_device_t* device);
int vfio_device_max_interrupts_get(vfio_device_t* device, uint32_t* count);
int vfio_device_interrupts_assign(vfio_device_t* device, uint32_t start, uint32_t count, int32_t* fds);
int vfio_device_interrupts_clear(vfio_device_t* device, uint32_t start, uint32_t count);
int vfio_device_interrupts_create(vfio_device_t* device, uint32_t start, uint32_t count, int32_t* fds);
int vfio_device_vendor_id_read(vfio_device_t* device, uint16_t* vendor_id);
int vfio_device_device_id_read(vfio_device_t* device, uint16_t* device_id);
int vfio_device_enable(vfio_device_t* device);

#endif  // __VFIO_UTILS_INCLUDE__
