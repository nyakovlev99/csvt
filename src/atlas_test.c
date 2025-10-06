#include "archer.h"
#include "pci_utils.h"
#include <stdio.h>

int run_main(int argc, char** argv) {

    pci_device_t    pci_devices[MAX_ARCHER_DEVICES];
    archer_t        archers[MAX_ARCHER_DEVICES];
    int             n_devices;

    if (archer_enumerate(pci_devices, archers, MAX_ARCHER_DEVICES, &n_devices) < 0) return -1;

    return 0;
}

int main(int argc, char** argv) {
    int ret = run_main(argc, argv);
    if (ret != 0) {
        printf("Command failed (%d)", ret);
    }
    return ret;
}
