#include "dma_global_csr.h"

#include "field_32.h"
#include "io_32.h"

#include <stdlib.h>
#include <unistd.h>

typedef struct {
    void* io;
    void* field_writeback_delay;
    void* field_version;
    void* field_reset;
} dma_global_csr_t;

int dma_global_csr_create(void* io, uint64_t offset, void** dma_global_csr) {
    *dma_global_csr = calloc(sizeof(dma_global_csr_t), 1);

    if (*dma_global_csr == NULL) return -1;
    dma_global_csr_t* _dma_global_csr = (dma_global_csr_t*)*dma_global_csr;

    _dma_global_csr->io = io;

    if (dma_global_csr_field_32_create(io, offset + 0x8,   0, 0xfffff,   &(_dma_global_csr->field_writeback_delay)) < 0) return -1;
    if (dma_global_csr_field_32_create(io, offset + 0x70,  0, 0xffffff,  &(_dma_global_csr->field_version        )) < 0) return -2;
    if (dma_global_csr_field_32_create(io, offset + 0x120, 0, 0b1,       &(_dma_global_csr->field_reset          )) < 0) return -3;

    return 0;
}

int dma_global_csr_set_writeback_delay(void* dma_global_csr, uint32_t delay) {
    dma_global_csr_t* _dma_global_csr = (dma_global_csr_t*)dma_global_csr;
    return dma_global_csr_field_32_write(_dma_global_csr->field_writeback_delay, delay);
}

int dma_global_csr_get_writeback_delay(void* dma_global_csr, uint32_t* delay) {
    dma_global_csr_t* _dma_global_csr = (dma_global_csr_t*)dma_global_csr;
    return dma_global_csr_field_32_read(_dma_global_csr->field_writeback_delay, delay);
}

int dma_global_csr_get_version(void* dma_global_csr, uint8_t* major, uint8_t* update, uint8_t* patch) {
    dma_global_csr_t* _dma_global_csr = (dma_global_csr_t*)dma_global_csr;
    uint32_t value;
    int ret = dma_global_csr_field_32_read(_dma_global_csr->field_version, &value);
    if (ret < 0) return ret;
    *patch  = value & 0xff;
    *update = (value >> 8) & 0xff;
    *major  = (value >> 16) & 0xff;
    return 0;
}

int dma_global_csr_reset(void* dma_global_csr) {
    dma_global_csr_t* _dma_global_csr = (dma_global_csr_t*)dma_global_csr;
    return dma_global_csr_field_32_write(_dma_global_csr->field_reset, 1);

    // Wait for FPGA to reset (at least until more explicit method is identified)
    usleep(100000);

    return 0;
}
