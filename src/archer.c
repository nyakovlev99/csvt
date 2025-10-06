#include "archer.h"
#include <stdio.h>

int archer_enumerate(
    pci_device_t* pci_devices,
    archer_t* archers,
    int max_devices,
    int* n_devices
) {
    if (vfio_init() < 0) return -1;

    if (pci_enumerate_devices(POSITRON_VENDOR_ID, pci_devices, max_devices, n_devices) < 0) return -2;

    printf("Detected %d devices\n", *n_devices);

    for (int i=0; i<*n_devices; i++) {
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

        if (vfio_group_create(&(archer->vfio_group), NULL, vfio_path) < 0) return -3;
        if (vfio_device_add(&(archer->vfio_device), &(archer->vfio_group), dbdf) < 0) return -4;
        if (vfio_device_region_attach(&(archer->vfio_device), DMA_BAR) < 0) return -5;
        if (vfio_device_region_attach(&(archer->vfio_device), CSR_BAR) < 0) return -6;
        if (vfio_device_region_attach(&(archer->vfio_device), HBM_BAR) < 0) return -7;
        if (vfio_device_enable(&(archer->vfio_device)) < 0) return -8;

        archer->id_dat      = (uint64_t*)((uint64_t)(archer->vfio_device.regions[CSR_BAR].mem) + 0x0);
        archer->wcmd        = (uint64_t*)((uint64_t)(archer->vfio_device.regions[CSR_BAR].mem) + 0x700200);
        archer->acmd        = (uint64_t*)((uint64_t)(archer->vfio_device.regions[CSR_BAR].mem) + 0x700300);
        archer->n_results   = (uint16_t*)((uint64_t)(archer->vfio_device.regions[CSR_BAR].mem) + 0x700400);
        archer->rmem_pop    = (uint32_t*)((uint64_t)(archer->vfio_device.regions[CSR_BAR].mem) + RMEM_BASE);
        for (int pchan_idx=0; pchan_idx<N_PCHANNELS; pchan_idx++) {
            archer->pchannels[pchan_idx] = (__m512i*)((uint64_t)(archer->vfio_device.regions[HBM_BAR].mem) + PCHANNEL_BASES[pchan_idx]);
        }

        dma_global_csr_create(&(archer->dma_global_csr), archer->vfio_device.regions[DMA_BAR].mem);

        dma_version_t dma_version;
        dma_global_csr_version(&(archer->dma_global_csr), &dma_version);

        // TODO: allocate a hugepage for DMA and start the ring

        printf(
            "Archer device: %s (vfio:%s); id_dat=%016lx, dma version: %02x.%02x.%02x\n",
            dbdf,
            pci_device->iommu_group,
            *archer->id_dat,
            dma_version.major, dma_version.minor, dma_version.patch
        );
    }
    return 0;
}
