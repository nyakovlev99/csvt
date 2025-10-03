#include "pci_utils.h"
#include "vfio_utils.h"
#include <stdio.h>
#include <immintrin.h>

#define POSITRON_VENDOR_ID 0x8200
#define MAX_DEVICES 16

typedef struct {
    pci_device_t*   pci_device;
    vfio_group_t    vfio_group;
    vfio_device_t   vfio_device;
} archer_t;

int run_main(int argc, char** argv) {
    printf("Running stress test...\n");

    int                 n_devices;
    vfio_container_t    ctr;
    pci_device_t        pci_devices     [MAX_DEVICES];
    archer_t            archers         [MAX_DEVICES];

    if (vfio_container_create(&ctr) < 0) return -1;
    
    if (pci_enumerate_devices(POSITRON_VENDOR_ID, pci_devices, MAX_DEVICES, &n_devices) < 0) return -2;

    printf("Detected %d devices\n", n_devices);

    for (int i=0; i<n_devices; i++) {
        pci_device_t*   pci_device = &(pci_devices[i]);
        archer_t*       archer = &(archers[i]);
        char            vfio_path[128];
        char            dbdf[32];

        sprintf(vfio_path, "/dev/vfio/%s", pci_device->iommu_group);
        sprintf(
            dbdf, 
            "%04x:%02x:%02x.%d",
            pci_device->domain,
            pci_device->bus,
            pci_device->slot,
            pci_device->function
        );

        printf("Connecting to %s (%s)\n", dbdf, vfio_path);

        if (vfio_group_create(&(archer->vfio_group), &ctr, vfio_path) < 0) return -3;
        if (vfio_device_add(&(archer->vfio_device), &(archer->vfio_group), dbdf) < 0) return -4;
        if (vfio_device_region_attach(&(archer->vfio_device), 2) < 0) return -5;
        if (vfio_device_enable(&(archer->vfio_device)) < 0) return -5;
    }

    return 0;
}

int main(int argc, char** argv) {
    int ret = run_main(argc, argv);
    if (ret != 0) {
        printf("Command failed (%d)", ret);
    }
    return ret;
}
