#include "pci_utils.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static struct pci_access* pci_access;

char* pci_link_speed_decode (int speed) {
    switch (speed) {
        case PCIE_SPEED_2_5GT:
            return "2.5GT/s";
        case PCIE_SPEED_5GT:
            return "5GT/s";
        case PCIE_SPEED_8GT:
            return "8GT/s";
        case PCIE_SPEED_16GT:
            return "16GT/s";
        case PCIE_SPEED_32GT:
            return "32GT/s";
        case PCIE_SPEED_64GT:
            return "64GT/s";
        default:
            return "unknown";
    }
}

static int pci_device_link_status_get (struct pci_dev* dev, uint16_t* link_speed, uint16_t* link_width) {
    // Would probably be better with something like this field, only available within pciutils
    // *link_speed = dev->rcd_link_status & PCI_EXP_LNKSTA_SPEED;
    // *link_speed = dev->rcd_link_status & PCI_EXP_LNKSTA_WIDTH;

    struct pci_cap* cap;
    uint16_t word;

    cap = dev->first_cap;
    while (cap) {
        if (cap->id == PCI_CAP_ID_EXP) {
            word = pci_read_word(dev, cap->addr + PCI_EXP_LNKSTA);
            *link_speed = word & PCI_EXP_LNKSTA_SPEED;
            *link_width = (word & PCI_EXP_LNKSTA_WIDTH) >> 4;
            return 0;
        }
        cap = cap->next;
    }
    printf("Failed to locate PCI Express capability for querying link state\n");
    return -1;
}

int pci_enumerate_devices(uint16_t vendor_id, pci_device_t* devices, int max_devices, int* n_devices) {
    struct pci_dev* dev;
    char* _iommu_group;
    *n_devices = 0;

    pci_access = pci_alloc();
    pci_init(pci_access);
    pci_scan_bus(pci_access);

    for (dev = pci_access->devices; dev && *n_devices < max_devices; dev = dev->next) {
        pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS | PCI_FILL_CAPS | PCI_FILL_IOMMU_GROUP);
        
        if (dev->vendor_id != vendor_id) continue;
        
        pci_device_t* device = &(devices[*n_devices]);
        
        device->domain      = dev->domain_16;
        device->bus         = dev->bus;
        device->slot        = dev->dev;
        device->function    = dev->func;
        device->vendor_id   = dev->vendor_id;
        device->device_id   = dev->device_id;
        
        device->link_detected = (pci_device_link_status_get(
            dev,
            &(device->link_speed),
            &(device->link_width)
        ) == 0);
        
        _iommu_group = pci_get_string_property(dev, PCI_FILL_IOMMU_GROUP);
        strncpy(device->iommu_group, _iommu_group, IOMMU_NAME_SIZE);
        
        *n_devices = *n_devices + 1;
    }

    pci_cleanup(pci_access);
    return 0;
}
