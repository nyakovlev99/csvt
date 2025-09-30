#include "dma_queue_csr.h"

#include "field_32.h"
#include "io_32.h"
#include "io_64.h"

#include <sys/epoll.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdio.h>

typedef struct {
    void* io;
    void* field_interrupt_enable;
    void* field_writeback_enable;
    void* field_enable;
    uint64_t ptr_start_addr;
    void* field_size;
    void* field_tail_ptr;
    void* field_head_ptr;
    void* field_completed_ptr;
    uint64_t ptr_consumed_head_addr;
    void* field_batch_delay;
    void* field_data_drop_err_status;
    void* field_data_err_avmm;
    void* field_data_err_avst;
    void* field_data_drop_err_count;
    void* field_payload_count;
    void* field_reset;

    // values for transfer access
    void* ring_base;
    uint16_t* completed_ptr;
    uint16_t* tail_ptr;

    uint32_t size;
    uint32_t completed;
    uint32_t last_tail;
    uint32_t tail;
    uint16_t ring_mask;

    int msi_fd;
    int listener_fd;
    int timeout;

    uint64_t vrb;

} dma_queue_csr_t;

int dma_queue_csr_create(void* io, uint64_t offset, void** dma_queue_csr) {
    *dma_queue_csr = calloc(sizeof(dma_queue_csr_t), 1);

    if (*dma_queue_csr == NULL) return -1;
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)*dma_queue_csr;

    _dma_queue_csr->io = io;

    _dma_queue_csr->ptr_start_addr          = offset + 0x8;
    _dma_queue_csr->ptr_consumed_head_addr  = offset + 0x20;

    if (dma_queue_csr_field_32_create(io, offset + 0x0,  9,  0b1,       &(_dma_queue_csr->field_interrupt_enable     )) < 0) return -1;
    if (dma_queue_csr_field_32_create(io, offset + 0x0,  8,  0b1,       &(_dma_queue_csr->field_writeback_enable     )) < 0) return -2;
    if (dma_queue_csr_field_32_create(io, offset + 0x0,  0,  0b1,       &(_dma_queue_csr->field_enable               )) < 0) return -3;
    if (dma_queue_csr_field_32_create(io, offset + 0x10, 0,  0b11111,   &(_dma_queue_csr->field_size                 )) < 0) return -4;
    if (dma_queue_csr_field_32_create(io, offset + 0x14, 0,  0xffff,    &(_dma_queue_csr->field_tail_ptr             )) < 0) return -5;
    if (dma_queue_csr_field_32_create(io, offset + 0x18, 0,  0xffff,    &(_dma_queue_csr->field_head_ptr             )) < 0) return -6;
    if (dma_queue_csr_field_32_create(io, offset + 0x1c, 0,  0xffff,    &(_dma_queue_csr->field_completed_ptr        )) < 0) return -7;
    if (dma_queue_csr_field_32_create(io, offset + 0x28, 0,  0xfffff,   &(_dma_queue_csr->field_batch_delay          )) < 0) return -8;
    if (dma_queue_csr_field_32_create(io, offset + 0x40, 20, 0b1,       &(_dma_queue_csr->field_data_drop_err_status )) < 0) return -9;
    if (dma_queue_csr_field_32_create(io, offset + 0x40, 17, 0b1,       &(_dma_queue_csr->field_data_err_avmm        )) < 0) return -10;
    if (dma_queue_csr_field_32_create(io, offset + 0x40, 16, 0b1,       &(_dma_queue_csr->field_data_err_avst        )) < 0) return -11;
    if (dma_queue_csr_field_32_create(io, offset + 0x40, 0,  0xffff,    &(_dma_queue_csr->field_data_drop_err_count  )) < 0) return -12;
    if (dma_queue_csr_field_32_create(io, offset + 0x44, 0,  0xfffff,   &(_dma_queue_csr->field_payload_count        )) < 0) return -13;
    if (dma_queue_csr_field_32_create(io, offset + 0x48, 0,  0b1,       &(_dma_queue_csr->field_reset                )) < 0) return -14;

    _dma_queue_csr->completed_ptr = (uint16_t*)((char*)(offset + 0x1c));
    _dma_queue_csr->tail_ptr = (uint16_t*)((char*)(offset + 0x14));

    return 0;
}

int dma_queue_csr_start_addr_set(void* dma_queue_csr, uint64_t start_addr) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_io_64_write(_dma_queue_csr->io, _dma_queue_csr->ptr_start_addr, start_addr);
}
int dma_queue_csr_start_addr_get(void* dma_queue_csr, uint64_t* start_addr) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_io_64_read(_dma_queue_csr->io, _dma_queue_csr->ptr_start_addr, start_addr);
}

int dma_queue_csr_consumed_head_addr_set(void* dma_queue_csr, uint64_t consumed_head_addr) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_io_64_write(_dma_queue_csr->io, _dma_queue_csr->ptr_consumed_head_addr, consumed_head_addr);
}
int dma_queue_csr_consumed_head_addr_get(void* dma_queue_csr, uint64_t* consumed_head_addr) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_io_64_read(_dma_queue_csr->io, _dma_queue_csr->ptr_consumed_head_addr, consumed_head_addr);
}

int dma_queue_csr_interrupt_enable(void* dma_queue_csr) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_write(_dma_queue_csr->field_interrupt_enable, 1);
}
int dma_queue_csr_interrupt_disable(void* dma_queue_csr) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_write(_dma_queue_csr->field_interrupt_enable, 0);
}
int dma_queue_csr_interrupt_enabled(void* dma_queue_csr, uint32_t* enabled) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_read(_dma_queue_csr->field_interrupt_enable, enabled);
}

int dma_queue_csr_writeback_enable(void* dma_queue_csr) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_write(_dma_queue_csr->field_writeback_enable, 1);
}
int dma_queue_csr_writeback_disable(void* dma_queue_csr) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_write(_dma_queue_csr->field_writeback_enable, 0);
}
int dma_queue_csr_writeback_enabled(void* dma_queue_csr, uint32_t* enabled) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_read(_dma_queue_csr->field_writeback_enable, enabled);
}

int dma_queue_csr_enable(void* dma_queue_csr) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_write(_dma_queue_csr->field_enable, 1);
}
int dma_queue_csr_disable(void* dma_queue_csr) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_write(_dma_queue_csr->field_enable, 0);
}
int dma_queue_csr_enabled(void* dma_queue_csr, uint32_t* enabled) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_read(_dma_queue_csr->field_enable, enabled);
}

int dma_queue_csr_size_set(void* dma_queue_csr, uint32_t size) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_write(_dma_queue_csr->field_size, size);
}
int dma_queue_csr_size_get(void* dma_queue_csr, uint32_t* size) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_read(_dma_queue_csr->field_size, size);
}

int dma_queue_csr_tail_ptr_set(void* dma_queue_csr, uint32_t tail_ptr) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_write(_dma_queue_csr->field_tail_ptr, tail_ptr);
}
int dma_queue_csr_tail_ptr_get(void* dma_queue_csr, uint32_t* tail_ptr) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_read(_dma_queue_csr->field_tail_ptr, tail_ptr);
}

int dma_queue_csr_head_ptr_get(void* dma_queue_csr, uint32_t* head_ptr) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_read(_dma_queue_csr->field_head_ptr, head_ptr);
}

int dma_queue_csr_completed_ptr_get(void* dma_queue_csr, uint32_t* completed_ptr) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_read(_dma_queue_csr->field_completed_ptr, completed_ptr);
}

int dma_queue_csr_batch_delay_set(void* dma_queue_csr, uint32_t batch_delay) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_write(_dma_queue_csr->field_batch_delay, batch_delay);
}
int dma_queue_csr_batch_delay_get(void* dma_queue_csr, uint32_t* batch_delay) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_read(_dma_queue_csr->field_batch_delay, batch_delay);
}

int dma_queue_csr_data_drop_err_status_set(void* dma_queue_csr, uint32_t data_drop_err_status) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_write(_dma_queue_csr->field_data_drop_err_status, data_drop_err_status);
}
int dma_queue_csr_data_drop_err_status_get(void* dma_queue_csr, uint32_t* data_drop_err_status) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_read(_dma_queue_csr->field_data_drop_err_status, data_drop_err_status);
}

int dma_queue_csr_data_err_avmm_set(void* dma_queue_csr, uint32_t data_err_avmm) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_write(_dma_queue_csr->field_data_err_avmm, data_err_avmm);
}
int dma_queue_csr_data_err_avmm_get(void* dma_queue_csr, uint32_t* data_err_avmm) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_read(_dma_queue_csr->field_data_err_avmm, data_err_avmm);
}

int dma_queue_csr_data_err_avst_set(void* dma_queue_csr, uint32_t data_err_avst) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_write(_dma_queue_csr->field_data_err_avst, data_err_avst);
}
int dma_queue_csr_data_err_avst_get(void* dma_queue_csr, uint32_t* data_err_avst) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_read(_dma_queue_csr->field_data_err_avst, data_err_avst);
}

int dma_queue_csr_data_drop_err_count_set(void* dma_queue_csr, uint32_t data_drop_err_count) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_write(_dma_queue_csr->field_data_drop_err_count, data_drop_err_count);
}
int dma_queue_csr_data_drop_err_count_get(void* dma_queue_csr, uint32_t* data_drop_err_count) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_read(_dma_queue_csr->field_data_drop_err_count, data_drop_err_count);
}

int dma_queue_csr_payload_count_set(void* dma_queue_csr, uint32_t payload_count) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_write(_dma_queue_csr->field_payload_count, payload_count);
}
int dma_queue_csr_payload_count_get(void* dma_queue_csr, uint32_t* payload_count) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_read(_dma_queue_csr->field_payload_count, payload_count);
}

int dma_queue_csr_reset_assert(void* dma_queue_csr) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_write(_dma_queue_csr->field_reset, 1);
}
int dma_queue_csr_reset_asserted(void* dma_queue_csr, uint32_t* asserted) {
    dma_queue_csr_t* _dma_queue_csr = (dma_queue_csr_t*)dma_queue_csr;
    return dma_queue_csr_field_32_read(_dma_queue_csr->field_reset, asserted);
}
int dma_queue_csr_reset(void* dma_queue_csr) {
    if (dma_queue_csr_reset_assert(dma_queue_csr) < 0) return -1;
    uint32_t asserted;
    while (1) {
        if (dma_queue_csr_reset_asserted(dma_queue_csr, &asserted) < 0) return -2;
        if (asserted == 0) return 0;
        usleep(1000);
    }
}

// == DMA Transfer Functionality ==
// (redundant calls that provide more offload to C-side APIs)

// // #define TRANSFER_MASK   0x7f    // used to find size of last transfer
// #define TRANSFER_MASK   0xff    // used to find size of last transfer
// #define TRANSFER_BYTES  256     // Maximum payload count of a single DMA transfer
// #define TRANSFER_SHIFT  8       // bit shift to divide by max transfers (2^8 = 256)
// #define TRANSFER_STRIDE 0x100   // stride to move src and dst addresses

#define TRANSFER_MASK   0x7ffff // used to find size of last transfer
#define TRANSFER_BYTES  524288  // Maximum payload count of a single DMA transfer
#define TRANSFER_SHIFT  19      // bit shift to divide by max transfers (2^8 = 256)
#define TRANSFER_STRIDE 0x80000 // stride to move src and dst addresses

// // For large transfers, unsupported in rmem
// #define TRANSFER_MASK   0xffff  // used to find size of last transfer
// #define TRANSFER_BYTES  524288  // Maximum payload count of a single DMA transfer
// #define TRANSFER_SHIFT  19      // bit shift to divide by max transfers (2^8 = 256)
// #define TRANSFER_STRIDE 0x80000 // stride to move src and dst addresses



// #define TRANSFER_BYTES  128     // Maximum payload count of a single DMA transfer
// #define TRANSFER_SHIFT  7       // bit shift to divide by max transfers (2^8 = 256)
// #define TRANSFER_STRIDE 0x080   // stride to move src and dst addresses

// #define MAX_DMA_MASK    0xff    // Mask for remainder bytes (ignored until remainders are supported)
// #define MAX_BATCH       120  // slight instability observed when loading weights
// #define MAX_BATCH       124  // slight instability observed when loading weights
#define MAX_BATCH       40

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

static void dma_descriptor_write(
    void*       mem,
    uint64_t    src_addr,
    uint64_t    dst_addr,
    uint64_t    payload_count,
    uint64_t    desc_idx,
    uint64_t    msix_enable,
    uint64_t    writeback_enable,
    uint64_t    rx_payload_count,
    uint64_t    sof,
    uint64_t    eof,
    uint64_t    desc_invalid,
    uint64_t    link
) {
    uint64_t* _mem = (uint64_t*)mem;

    _mem[0] = src_addr;
    _mem[1] = dst_addr;
    _mem[2] = 
        (payload_count & 0xfffff) |
        ((desc_idx & 0xffff) << 32) |
        ((msix_enable & 0b1) << 48) |
        ((writeback_enable & 0b1) << 49);
    _mem[3] = 
        (rx_payload_count & 0xfffff) |
        ((sof & 0b1) << 30) |
        ((eof & 0b1) << 31) |
        ((desc_invalid & 0b1) << 62) |
        ((link & 0b1) << 63);
}

int dma_queue_csr_start(void* dma_queue_csr, void* ring_base, uint64_t virtual_ring_base, uint32_t size, int msi_fd, int timeout) {

    dma_queue_csr_t* q = (dma_queue_csr_t*)dma_queue_csr;
    q->ring_base = ring_base;
    q->size = (1 << size);
    q->tail = 1;
    q->last_tail = 1;
    q->completed = 1;
    q->ring_mask = (1 << size) - 1;
    q->listener_fd = epoll_create1(0);
    q->msi_fd = msi_fd;
    q->timeout = timeout;
    q->vrb = virtual_ring_base;

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.ptr = NULL;
    if (epoll_ctl(q->listener_fd, EPOLL_CTL_ADD, q->msi_fd, &event) < 0) return -1;

    // Chaining descriptors (for page-based ring)
    for (int i = 127; i < q->size; i += 128) {
        // TODO: write a linking descriptor to (i + 127) (to support size >7)
        // dma_descriptor_write(
        //     &(ring_base[q->tail << 2]),     // mem
        //     src_addr,                       // src_addr
        //     dst_addr,                       // dst_addr
        //     TRANSFER_BYTES,                 // payload_count
        //     (uint64_t)q->tail,              // desc_idx
        //     0,                              // msix_enable
        //     0,                              // writeback_enable
        //     0,                              // rx_payload_count
        //     0,                              // sof
        //     0,                              // eof
        //     0,                              // desc_invalid
        //     0                               // link
        // );
    }

    // Last chaining descriptor (points to the start of the ring)
    uint64_t* _ring_base = (uint64_t*)(q->ring_base);
    dma_descriptor_write(
        &(_ring_base[(q->size - 1) << 2]),     // mem
        virtual_ring_base,              // src_addr
        0,                              // dst_addr
        TRANSFER_BYTES,                 // payload_count
        (uint64_t)q->tail,              // desc_idx
        1,                              // msix_enable
        0,                              // writeback_enable
        0,                              // rx_payload_count
        0,                              // sof
        0,                              // eof
        0,                              // desc_invalid
        1                               // link
    );

    if (dma_queue_csr_reset             (dma_queue_csr)                     < 0)    return -2;
    if (dma_queue_csr_start_addr_set    (dma_queue_csr, virtual_ring_base)  < 0)    return -3;
    if (dma_queue_csr_size_set          (dma_queue_csr, size)               < 0)    return -4;
    if (dma_queue_csr_batch_delay_set   (dma_queue_csr, 1)                  < 0)    return -5;
    if (dma_queue_csr_interrupt_enable  (dma_queue_csr)                     < 0)    return -6;
    if (dma_queue_csr_enable            (dma_queue_csr)                     < 0)    return -7;

    return 0;
}

static uint32_t get_room(dma_queue_csr_t* q) {
    // TODO: add good support for other ring sizes
    uint32_t completed = (q->completed > q->tail) ? q->completed : (q->completed + 127);
    return MIN(completed - q->tail - 1, MAX_BATCH);
}

static void pull_completed(dma_queue_csr_t* q) {
    q->completed = *(q->completed_ptr);
}

static void increment_tail (dma_queue_csr_t* q) {
    // // shift one extra if the next index is a chaining descriptor
    // uint32_t inc = unlikely((q->last_tail & 0x7f) == 0x7f) ? 2 : 1;
    // q->last_tail = q->tail;
    // q->tail = ((q->tail + inc) & q->ring_mask);
    // TODO: add good support for other ring sizes
    q->last_tail = q->tail;
    if (unlikely(q->tail == 128)) {
        q->tail = 1;
    } else {
        q->tail++;
    }
}

static int invoke_ring(dma_queue_csr_t* q) {

    // printf("Invoke\n");

    *(q->tail_ptr) = q->last_tail;
    asm volatile("" ::: "memory");  // TODO: possibly replace all these barriers with just a volatile flag?

    // // Method 1: use MSI interrupt
    // // Using epoll_wait suppresses misbehavior from MSI - but at an enormous tax. Can't confirm yet if it's my fault or not.

    // struct epoll_event events;
    // int num_events = epoll_wait(
    //     q->listener_fd,
    //     &events,
    //     1,  // max_events
    //     q->timeout
    // );
    // if (num_events < 0) return -1;

    // // Read out the epoll event to lower the counter
    // uint64_t count;
    // ssize_t num_read = read(q->msi_fd, &count, sizeof(uint64_t));
    // (void)(num_read);

    // return 0;

    // Method 2 - poll for completion ptr.
    int i;
    for (i=0; i<10000; i++) {
        if (*(q->completed_ptr) == *(q->tail_ptr)) return 0;
        asm volatile("" ::: "memory");
        // usleep(1000);
    }
    printf("Ring timeout encountered: <%u:%u>\n", *(q->tail_ptr), *(q->completed_ptr));
    return -1;
}

// NOTE: at the moment, we expect num_bytes to be cleanly divisible by 256 (8 rows of results).
// This is purely to make the logic easier - redesign if more granular transfers become needed.
int dma_queue_csr_transfer(void* dma_queue_csr, uint64_t dst_addr, uint64_t src_addr, uint64_t num_bytes) {
    dma_queue_csr_t* q = (dma_queue_csr_t*)dma_queue_csr;
    uint64_t* ring_base = (uint64_t*)(q->ring_base);

    uint32_t room = 0;
    uint64_t num_descs = ((num_bytes + TRANSFER_BYTES - 1) >> TRANSFER_SHIFT);
    uint64_t transfer_remainder = num_bytes & TRANSFER_MASK;
    if (transfer_remainder == 0) transfer_remainder = TRANSFER_BYTES;
    uint32_t i;

    // printf("DMAing out %lu bytes using %lu descriptors\n", num_bytes, num_descs);
    // fflush(stdout);

    while (likely(num_descs > 1)) {
        pull_completed(q);
        room = get_room(q);
        uint64_t todo = MIN(num_descs - 1, room);
        // todo = MIN(todo, 30);
        for (i = 0; i < todo; i++) {

            dma_descriptor_write(
                &(ring_base[(q->tail - 1) << 2]),     // mem
                src_addr,                       // src_addr
                dst_addr,                       // dst_addr
                TRANSFER_BYTES,                 // payload_count
                (uint64_t)q->tail,              // desc_idx
                0,                              // msix_enable
                0,                              // writeback_enable
                0,                              // rx_payload_count
                0,                              // sof
                0,                              // eof
                0,                              // desc_invalid
                0                               // link
            );
            increment_tail(q);

            src_addr += TRANSFER_STRIDE;
            dst_addr += TRANSFER_STRIDE;
        }

        if (invoke_ring(q) < 0) return -1;
        num_descs -= todo;
        room -= todo;

        // printf("xferred %u (remaining: %u) <%u:%u>\n", todo, num_descs, *(q->tail_ptr), *(q->completed_ptr));
        // fflush(stdout);
        // usleep(10000);
    }

    dma_descriptor_write(
        &(ring_base[(q->tail - 1) << 2]),     // mem
        src_addr,                       // src_addr
        dst_addr,                       // dst_addr
        transfer_remainder,             // payload_count
        (uint64_t)q->tail,              // desc_idx
        0,                              // msix_enable
        0,                              // writeback_enable
        0,                              // rx_payload_count
        0,                              // sof
        0,                              // eof
        0,                              // desc_invalid
        0                               // link
    );
    increment_tail(q);
    if (invoke_ring(q) < 0) return -1;

    return 0;

}
