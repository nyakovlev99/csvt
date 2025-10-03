#include "pci_utils.h"
#include "vfio_utils.h"
#include <stdio.h>

#define MAX_DEVICES 16

int run_main(int argc, char** argv) {
    printf("TODO: verify matmul\n");

    vfio_container_t ctr;
    if (vfio_container_create(&ctr) < 0) return -1;

    pci_device_t devices[MAX_DEVICES];

    return 0;
}

int main(int argc, char** argv) {
    int ret = run_main(argc, argv);
    if (ret != 0) {
        printf("Command failed (%d)", ret);
    }
    return ret;
}
