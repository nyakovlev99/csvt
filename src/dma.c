#include "dma.h"
#include "vfio_utils.h"

#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#define MIN(a,b)    (((a)<(b))?(a):(b))
#define MAX(a,b)    (((a)>(b))?(a):(b))
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define DMA_GLOBAL_CSR_BASE 0x200000
#define BATCH_DELAY         1
#define TRANSFER_MASK       0x7ffff
#define TRANSFER_BYTES      524288
#define TRANSFER_SHIFT      19
#define TRANSFER_STRIDE     0x80000
#define MAX_BATCH           40

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

int dma_pool_create(
    dma_pool_t* pool,
    vfio_group_t* vfio_group,
    uint64_t virtual_base,
    uint64_t buffer_offset
) {
    pool->virtual_base = virtual_base;
    printf("1\n");
    pool->mem = mmap(
        0,
        DMA_POOL_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
        0,
        0
    );
    if (vfio_group_map_create(
        vfio_group,
        pool->mem,
        DMA_POOL_SIZE,
        virtual_base
    )) return -1;
    return 0;
}

void dma_queue_csr_create(dma_queue_csr_t* csr, void* bar, uint64_t base, dma_pool_t* pool) {
    csr->reg_start_addr         = (uint64_t*)((uint64_t)bar + base + 0x08);
    csr->reg_consumed_head_addr = (uint64_t*)((uint64_t)bar + base + 0x20);
    csr->reg_enable             = (uint32_t*)((uint64_t)bar + base + 0x00);
    csr->reg_size               = (uint32_t*)((uint64_t)bar + base + 0x10);
    csr->reg_tail_ptr           = (uint32_t*)((uint64_t)bar + base + 0x14);
    csr->reg_head_ptr           = (uint32_t*)((uint64_t)bar + base + 0x18);
    csr->reg_completed_ptr      = (uint32_t*)((uint64_t)bar + base + 0x1c);
    csr->reg_batch_delay        = (uint32_t*)((uint64_t)bar + base + 0x28);
    csr->reg_error_counters     = (uint32_t*)((uint64_t)bar + base + 0x40);
    csr->reg_payload_count      = (uint32_t*)((uint64_t)bar + base + 0x44);
    csr->reg_reset              = (uint32_t*)((uint64_t)bar + base + 0x48);

    csr->is_d2h = base < DMA_H2D_QUEUE_CSR_BASE;
    csr->pool = pool;
}

int dma_queue_csr_reset(dma_queue_csr_t* csr) {
    *csr->reg_reset = 1;
    for (int i=0; i<1000; i++) {
        if (*csr->reg_reset == 0) return 0;
        usleep(1000);
    }
    return -1;
}

static void dma_descriptor_write(
    dma_queue_csr_t*    csr,
    int                 index,
    uint64_t            src_addr,
    uint64_t            dst_addr,
    uint64_t            payload_count,
    uint64_t            desc_idx,
    uint64_t            msix_enable,
    uint64_t            writeback_enable,
    uint64_t            rx_payload_count,
    uint64_t            sof,
    uint64_t            eof,
    uint64_t            desc_invalid,
    uint64_t            link
) {
    volatile uint64_t* slot = &(csr->ring[index * 4]);
    slot[0] = src_addr;
    slot[1] = dst_addr;
    slot[2] = 
        (payload_count & 0xfffff) |
        ((desc_idx & 0xffff) << 32) |
        ((msix_enable & 0b1) << 48) |
        ((writeback_enable & 0b1) << 49);
    slot[3] = 
        (rx_payload_count & 0xfffff) |
        ((sof & 0b1) << 30) |
        ((eof & 0b1) << 31) |
        ((desc_invalid & 0b1) << 62) |
        ((link & 0b1) << 63);
}

int dma_queue_csr_start(
    dma_queue_csr_t* csr,
    uint64_t ring_offset,
    uint32_t ring_width
) {
    csr->size = (1 << ring_width);
    csr->ring = (uint64_t*)((uint64_t)(csr->pool->mem) + ring_offset);
    csr->tail = 1;
    csr->last_tail = 1;
    csr->completed = 1;
    csr->ring_mask = (1 << ring_width) - 1;

    // Last chaining descriptor (points to the start of the ring)
    dma_descriptor_write(
        csr,
        csr->size,
        csr->pool->virtual_base + ring_offset,
        0,
        TRANSFER_BYTES,
        (uint64_t)csr->tail,
        1,
        0,
        0,
        0,
        0,
        0,
        1
    );

    if (dma_queue_csr_reset(csr) < 0) return -2;
    *csr->reg_start_addr    = csr->pool->virtual_base + ring_offset;
    *csr->reg_size          = ring_width;
    *csr->reg_batch_delay   = BATCH_DELAY;
    *csr->reg_enable        = 1;

    return 0;
}

static uint32_t get_room(dma_queue_csr_t* csr) {
    uint32_t completed = (csr->completed > csr->tail) ? csr->completed : (csr->completed + csr->size - 1);
    return MIN(completed - csr->tail - 1, MAX_BATCH);
}

static void pull_completed(dma_queue_csr_t* csr) {
    csr->completed = *csr->reg_completed_ptr;
}

static void increment_tail (dma_queue_csr_t* csr) {
    csr->last_tail = csr->tail;
    if (unlikely(csr->tail == csr->size)) {
        csr->tail = 1;
    } else {
        csr->tail++;
    }
}

static void commit(dma_queue_csr_t* csr) {
    *csr->reg_tail_ptr = csr->last_tail;
}

int dma_queue_csr_flush(dma_queue_csr_t* csr) {
    int i;
    for (i=0; i<10000; i++) {
        if (*csr->reg_completed_ptr == *csr->reg_tail_ptr) return 0;
    }
    printf("Ring timeout encountered: <%u:%u>\n", *csr->reg_tail_ptr, *csr->reg_completed_ptr);
    return -1;
}

int dma_queue_csr_transfer(
    dma_queue_csr_t* csr,
    uint64_t dst_addr,
    uint64_t src_addr,
    uint64_t num_bytes
) {
    if (csr->is_d2h) {
        dst_addr = dst_addr - ((uint64_t)csr->pool->mem) + csr->pool->buffer_offset;
    } else {
        src_addr = src_addr - ((uint64_t)csr->pool->mem) + csr->pool->buffer_offset;
    }

    uint32_t room = 0;
    uint64_t num_descs = ((num_bytes + TRANSFER_BYTES - 1) >> TRANSFER_SHIFT);
    uint64_t transfer_remainder = num_bytes & TRANSFER_MASK;
    if (transfer_remainder == 0) transfer_remainder = TRANSFER_BYTES;
    uint32_t i;

    // printf("DMAing out %lu bytes using %lu descriptors\n", num_bytes, num_descs);
    // fflush(stdout);

    while (likely(num_descs > 1)) {
        pull_completed(csr);
        room = get_room(csr);
        uint64_t todo = MIN(num_descs - 1, room);
        for (i = 0; i < todo; i++) {

            dma_descriptor_write(
                csr,
                csr->tail - 1,
                src_addr,
                dst_addr,
                TRANSFER_BYTES,
                (uint64_t)csr->tail,
                0,
                0,
                0,
                0,
                0,
                0,
                0
            );
            increment_tail(csr);

            src_addr += TRANSFER_STRIDE;
            dst_addr += TRANSFER_STRIDE;
        }

        commit(csr);
        num_descs -= todo;
        room -= todo;

        // printf("xferred %u (remaining: %u) <%u:%u>\n", todo, num_descs, *(q->tail_ptr), *(q->completed_ptr));
        // fflush(stdout);
        // usleep(10000);
    }

    dma_descriptor_write(
        csr,
        csr->tail - 1,
        src_addr,
        dst_addr,
        transfer_remainder,
        (uint64_t)csr->tail,
        0,
        0,
        0,
        0,
        0,
        0,
        0
    );
    increment_tail(csr);
    commit(csr);

    return 0;

}
