#ifndef __ARCHER_INCLUDE__
#define __ARCHER_INCLUDE__

#include "vfio_group.h"

#include <stdint.h>

typedef struct {
    uint64_t* reg_weights_cmd;
} archer_t;

int archer_create(archer_t* archer, device_t* device) {
}

#endif  // __ARCHER_INCLUDE__
