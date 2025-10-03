#ifndef __DMA_INCLUDE__
#define __DMA_INCLUDE__

#include <stdint.h>

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
    // void* io;
    // void* field_interrupt_enable;
    // void* field_writeback_enable;
    // void* field_enable;
    // uint64_t ptr_start_addr;
    // void* field_size;
    // void* field_tail_ptr;
    // void* field_head_ptr;
    // void* field_completed_ptr;
    // uint64_t ptr_consumed_head_addr;
    // void* field_batch_delay;
    // void* field_data_drop_err_status;
    // void* field_data_err_avmm;
    // void* field_data_err_avst;
    // void* field_data_drop_err_count;
    // void* field_payload_count;
    // void* field_reset;

    // // values for transfer access
    // void* ring_base;
    // uint16_t* completed_ptr;
    // uint16_t* tail_ptr;

    // uint32_t size;
    // uint32_t completed;
    // uint32_t last_tail;
    // uint32_t tail;
    // uint16_t ring_mask;
} dma_queue_csr_t;

void dma_global_csr_create(dma_global_csr_t* csr, void* mem);
void dma_global_csr_version(dma_global_csr_t* csr, dma_version_t* version);
int dma_global_csr_reset(dma_global_csr_t* csr);

void dma_queue_csr_create(dma_queue_csr_t* csr, void* mem, uint64_t base);

#endif  // __DMA_INCLUDE__
