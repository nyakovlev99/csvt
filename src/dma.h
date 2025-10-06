#ifndef __DMA_INCLUDE__
#define __DMA_INCLUDE__

#include "vfio_utils.h"

#include <stdint.h>
#include <stdbool.h>

#define DMA_D2H_QUEUE_CSR_BASE  0x0
#define DMA_H2D_QUEUE_CSR_BASE  0x80000

typedef struct {
    volatile uint32_t* writeback_delay;
    volatile uint32_t* version;
    volatile uint32_t* reset;
} dma_global_csr_t;

typedef struct {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
} dma_version_t;

typedef struct {

    volatile uint64_t* reg_start_addr;
    volatile uint64_t* reg_consumed_head_addr;
    volatile uint32_t* reg_enable;
    volatile uint32_t* reg_size;
    volatile uint32_t* reg_tail_ptr;
    volatile uint32_t* reg_head_ptr;
    volatile uint32_t* reg_completed_ptr;
    volatile uint32_t* reg_batch_delay;
    volatile uint32_t* reg_error_counters;
    volatile uint32_t* reg_payload_count;
    volatile uint32_t* reg_reset;

    void* pool;
    void* buffer;

    volatile uint64_t* ring;
    uint32_t           size;
    uint32_t           completed;
    uint32_t           last_tail;
    uint32_t           tail;
    uint16_t           ring_mask;
    uint64_t           virtual_ring_base;
    bool               is_d2h;

} dma_queue_csr_t;

void dma_global_csr_create(dma_global_csr_t* csr, void* mem);
void dma_global_csr_version(dma_global_csr_t* csr, dma_version_t* version);
int  dma_global_csr_reset(dma_global_csr_t* csr);

void dma_queue_csr_create(dma_queue_csr_t* csr, void* bar, uint64_t base);
int dma_queue_csr_start(dma_queue_csr_t* csr, vfio_group_t* vfio_group, uint32_t ring_width);
int dma_queue_csr_flush(dma_queue_csr_t* csr);
int dma_queue_csr_reset(dma_queue_csr_t* csr);
int dma_queue_csr_transfer(
    dma_queue_csr_t* csr,
    uint64_t dst_addr,
    uint64_t src_addr,
    uint64_t num_bytes
);

#endif  // __DMA_INCLUDE__
