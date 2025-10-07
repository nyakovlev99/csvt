#ifndef __ARCHER_INCLUDE__
#define __ARCHER_INCLUDE__

#include "dma.h"
#include "pci_utils.h"
#include "vfio_utils.h"

#include <stdint.h>
#include <immintrin.h>

#define POSITRON_VENDOR_ID  0x8200
#define MAX_ARCHER_DEVICES  16
#define DMA_BAR             0
#define CSR_BAR             2
#define HBM_BAR             4
#define AMEM_BASE           0x30000000
#define RMEM_BASE           0x20000000
#define N_AMEM_WORDS        2048
#define VIRT_BAR_BASE_2     0x4000000000
#define VIRT_BAR_BASE_4     0x8000000000
#define DMA_RING_WIDTH      7
#define N_DMA_RINGS         2
#define DMA_RING_SIZE       ((1 << DMA_RING_WIDTH) * DMA_DESCRIPTOR_SIZE)
#define POOL_BUFFER_OFFSET  (DMA_RING_SIZE * N_DMA_RINGS)

#define N_PCHANNELS 32
static uint64_t PCHANNEL_BASES[N_PCHANNELS] = {
    0x000000000,
    0x048000000,
    0x090000000,
    0x0d8000000,
    0x120000000,
    0x168000000,
    0x1b0000000,
    0x1f8000000,
    0x240000000,
    0x288000000,
    0x2d0000000,
    0x318000000,
    0x360000000,
    0x3a8000000,
    0x3f0000000,
    0x438000000,
    0x800000000,
    0x848000000,
    0x890000000,
    0x8d8000000,
    0x920000000,
    0x968000000,
    0x9b0000000,
    0x9f8000000,
    0xa40000000,
    0xa88000000,
    0xad0000000,
    0xb18000000,
    0xb60000000,
    0xba8000000,
    0xbf0000000,
    0xc38000000,
};

typedef struct {
    pci_device_t*       pci_device;
    vfio_group_t        vfio_group;
    vfio_device_t       vfio_device;
    volatile uint64_t*  id_dat;
    volatile uint64_t*  wcmd;
    volatile uint64_t*  acmd;
    volatile uint16_t*  n_results;
    volatile uint32_t*  rmem_pop;
    dma_global_csr_t    dma_global_csr;
    dma_pool_t          dma_pool;
    dma_queue_csr_t     dma_h2d;
    dma_queue_csr_t     dma_d2h;
    __m512i*            pchannels[N_PCHANNELS];
} archer_t;

int archer_enumerate(
    pci_device_t* pci_devices,
    archer_t* archers,
    int max_devices,
    int* n_devices
);

#endif  // __ARCHER_INCLUDE__
