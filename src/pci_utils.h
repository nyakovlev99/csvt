#ifndef __PCI_UTILS_INCLUDE__
#define __PCI_UTILS_INCLUDE__

#include <pci/pci.h>
#include <stdint.h>
#include <stdbool.h>

#define PCIE_SPEED_2_5GT    1
#define PCIE_SPEED_5GT      2
#define PCIE_SPEED_8GT      3
#define PCIE_SPEED_16GT     4
#define PCIE_SPEED_32GT     5
#define PCIE_SPEED_64GT     6

#define IOMMU_NAME_SIZE   32

typedef struct {
    uint16_t            domain;
    uint8_t             bus;
    uint8_t             slot;
    uint8_t             function;
    uint16_t            vendor_id;
    uint16_t            device_id;
    bool                link_detected;
    uint16_t            link_speed;
    uint16_t            link_width;
    char                iommu_group[IOMMU_NAME_SIZE];
} pci_device_t;

char*   pci_link_speed_decode       (int speed);
int     pci_enumerate_devices(uint16_t vendor_id, pci_device_t* devices, int max_devices, int* n_devices);

#endif  // __PCI_UTILS_INCLUDE__
