#include <unistd.h>
#include "dma.h"

#define DMA_GLOBAL_CSR_BASE     0x200000
#define DMA_D2H_QUEUE_CSR_BASE  0x0
#define DMA_H2D_QUEUE_CSR_BASE  0x80000

void dma_global_csr_create(dma_global_csr_t* csr, void* bar) {
    csr->writeback_delay    = (uint32_t*)((uint64_t)bar + DMA_GLOBAL_CSR_BASE + 0x8);
    csr->version            = (uint32_t*)((uint64_t)bar + DMA_GLOBAL_CSR_BASE + 0x70);
    csr->reset              = (uint32_t*)((uint64_t)bar + DMA_GLOBAL_CSR_BASE + 0x120);
}

void dma_global_csr_version(dma_global_csr_t* csr, dma_version_t* version) {
    uint32_t value = *csr->version;
    version->patch  = value & 0xff;
    version->minor  = (value >> 8) & 0xff;
    version->major  = (value >> 16) & 0xff;
}

int dma_global_csr_reset(dma_global_csr_t* csr) {
    *csr->reset = 1;
    usleep(100000);
    return 0;
}
