#include "dma.h"
#include "pci_utils.h"
#include "vfio_utils.h"
#include <stdint.h>
#include <stdio.h>
#include <immintrin.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define POSITRON_VENDOR_ID  0x8200
#define MAX_DEVICES         16
#define DMA_BAR             0
#define CSR_BAR             2
#define HBM_BAR             4
#define AMEM_BASE           0x30000000
#define RMEM_BASE           0x20000000
#define N_AMEM_WORDS        2048
#define VIRT_BAR_BASE_2     0x4000000000
#define VIRT_BAR_BASE_4     0x8000000000

// #define WCMD 0x40000a0000000000  
// #define WCMD 0x40000a0000000000
#define WCMD 0x4000060000000000  // best
// #define WCMD 0x0040040400000000
// #define WCMD 0x0040040400000000

// #define ACMD 0x0200028000010000
// #define ACMD 0x0200028000080000  // Batch 8
#define ACMD 0x0200018000010000  // best
// #define ACMD 0x0200018000010000
// #define ACMD 0x0200018000040000

// #define RESULTS_PER_OP 16
// #define RESULTS_PER_OP 128
#define RESULTS_PER_OP 8  // best
// #define RESULTS_PER_OP 8
// #define RESULTS_PER_OP 32

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
    __m512i*            pchannels[N_PCHANNELS];
} archer_t;

static pthread_mutex_t stdout_lock;

void* run_archer_threaded(void* arg) {
    archer_t* archer = (archer_t*)arg;

    pthread_mutex_lock(&stdout_lock);
    printf("Loading A-mem...\n");
    pthread_mutex_unlock(&stdout_lock);
    __m512i amem_pattern_1 = _mm512_set1_epi16(0xaaaa);
    __m512i amem_pattern_2 = _mm512_set1_epi16(0x5555);
    __m512i* amem = (__m512i*)((uint64_t)(archer->vfio_device.regions[2].mem) + AMEM_BASE);
    for (int i=0; i<N_AMEM_WORDS; i++) {
        // _mm512_store_si512(&(amem[i]), ((i / 16) % 2 == 0) ? amem_pattern_1 : amem_pattern_2);
        _mm512_store_si512(&(amem[i]), ((i / 4) % 2 == 0) ? amem_pattern_1 : amem_pattern_2);
    }

    // pthread_mutex_lock(&stdout_lock);
    // printf("Loading HBM...\n");
    // pthread_mutex_unlock(&stdout_lock);
    // __m512i hbm_pattern_1 = _mm512_set1_epi16(0xaaaa);
    // __m512i hbm_pattern_2 = _mm512_set1_epi16(0x5555);
    // for (int i=0; i<N_PCHANNELS; i++) {
    //     __m512i* pchan = archer->pchannels[i];
    //     for (int j=0; j<131072; j++) {
    //         _mm512_store_si512(&(pchan[j]), (i % 2 == 0) ? hbm_pattern_1 : hbm_pattern_2);
    //     }
    // }

    pthread_mutex_lock(&stdout_lock);
    printf("Running workload...\n");
    pthread_mutex_unlock(&stdout_lock);

    // while (1) {
    //     *archer->wcmd = WCMD;
    //     *archer->acmd = ACMD;
    //     while (*archer->n_results < RESULTS_PER_OP) {}
    //     for (int i=0; i<RESULTS_PER_OP; i++) *archer->rmem_pop;
    // }

    int n_inflight = 0;
    int n_total = RESULTS_PER_OP * 32;
    while (1) {
        while (n_inflight < n_total) {
            *archer->wcmd = WCMD;
            *archer->acmd = ACMD;
            n_inflight += RESULTS_PER_OP;
        }
        uint16_t n_results = *archer->n_results;
        for (int i=0; i<n_results; i++) *archer->rmem_pop;
        n_inflight -= n_results;
    }

    pthread_exit(NULL);
}

int run_main(int argc, char** argv) {
    printf("Checking system requirements...\n");

    char cmd_output[1024];
    FILE* fp = popen("cat /proc/meminfo | grep HugePages_Total | cut -d: -f2 | awk '{$1=$1}1'", "r");
    if (fgets(cmd_output, sizeof(cmd_output), fp) == NULL) {
        fprintf(stderr, "Failed to read hugepage output\n");
        return -1;
    }
    pclose(fp);
    int n_hugepages = atoi(cmd_output);
    printf("Hugepages: %d\n", n_hugepages);
    if (n_hugepages <= 0) {
        fprintf(stderr, "Invalid hugepages: %d\n", n_hugepages);
        return -2;
    }

    printf("Running stress test...\n");

    int                 n_devices;
    vfio_container_t    ctr;
    pci_device_t        pci_devices     [MAX_DEVICES];
    archer_t            archers         [MAX_DEVICES];

    pthread_mutex_init(&stdout_lock, NULL);

    if (vfio_container_create(&ctr) < 0) return -1;
    
    if (pci_enumerate_devices(POSITRON_VENDOR_ID, pci_devices, MAX_DEVICES, &n_devices) < 0) return -2;

    printf("Detected %d devices\n", n_devices);

    for (int i=0; i<n_devices; i++) {
        pci_device_t*   pci_device = &(pci_devices[i]);
        archer_t*       archer = &(archers[i]);
        char            vfio_path[128];
        char            dbdf[32];

        sprintf(vfio_path, "/dev/vfio/%s", pci_device->iommu_group);
        sprintf(
            dbdf, 
            "%04x:%02x:%02x.%d",
            pci_device->domain,
            pci_device->bus,
            pci_device->slot,
            pci_device->function
        );

        char cmd[1024];
        snprintf(cmd, 1024, "basename `readlink /sys/bus/pci/devices/%s/driver`", dbdf);
        fp = popen(cmd, "r");
        if (fgets(cmd_output, sizeof(cmd_output), fp) == NULL) {
            fprintf(stderr, "Failed to detect driver for %s; is it bound to one?\n", dbdf);
            return -1;
        }
        pclose(fp);
        printf("Device %s is bound to driver: %s\n", dbdf, cmd_output);
        if (strncmp(cmd_output, "vfio-pci", 8) != 0) {
            fprintf(stderr, "Device %s has incorrect driver %s\n", dbdf, cmd_output);
            return -2;
        }

        printf("Connecting to %s (%s)\n", dbdf, vfio_path);

        if (vfio_group_create(&(archer->vfio_group), &ctr, vfio_path) < 0) return -3;
        if (vfio_device_add(&(archer->vfio_device), &(archer->vfio_group), dbdf) < 0) return -4;
        if (vfio_device_region_attach(&(archer->vfio_device), DMA_BAR) < 0) return -5;
        if (vfio_device_region_attach(&(archer->vfio_device), CSR_BAR) < 0) return -6;
        if (vfio_device_region_attach(&(archer->vfio_device), HBM_BAR) < 0) return -7;
        if (vfio_device_enable(&(archer->vfio_device)) < 0) return -8;

        archer->id_dat      = (uint64_t*)((uint64_t)(archer->vfio_device.regions[CSR_BAR].mem) + 0x0);
        archer->wcmd        = (uint64_t*)((uint64_t)(archer->vfio_device.regions[CSR_BAR].mem) + 0x700200);
        archer->acmd        = (uint64_t*)((uint64_t)(archer->vfio_device.regions[CSR_BAR].mem) + 0x700300);
        archer->n_results   = (uint16_t*)((uint64_t)(archer->vfio_device.regions[CSR_BAR].mem) + 0x700400);
        archer->rmem_pop    = (uint32_t*)((uint64_t)(archer->vfio_device.regions[CSR_BAR].mem) + RMEM_BASE);
        // for (int pchan_idx=0; pchan_idx<N_PCHANNELS; pchan_idx++) {
        //     archer->pchannels[pchan_idx] = (__m512i*)((uint64_t)(archer->vfio_device.regions[HBM_BAR].mem) + PCHANNEL_BASES[pchan_idx]);
        // }

        dma_global_csr_create(&(archer->dma_global_csr), archer->vfio_device.regions[DMA_BAR].mem);

        uint64_t id_dat = *archer->id_dat;
        printf("ID_DAT: %016lx\n", id_dat);
        if (id_dat == 0xffffffffffffffff) {
            fprintf(stderr, "Invalid ID_DAT; bitfile may need to be reloaded.\n");
            return -8;
        }
        dma_version_t dma_version;
        dma_global_csr_version(&(archer->dma_global_csr), &dma_version);
        printf("DMA Version: %02x.%02x.%02x\n", dma_version.major, dma_version.minor, dma_version.patch);
    }

    pthread_t threads[MAX_DEVICES];
    for (int i=0; i<n_devices; i++) {
        archer_t* archer = &(archers[i]);
        pthread_create(&(threads[i]), NULL, run_archer_threaded, archer);
    }

    while (1) {
        sleep(1);
    }

    return 0;
}

int main(int argc, char** argv) {
    int ret = run_main(argc, argv);
    if (ret != 0) {
        printf("Command failed (%d)\n", ret);
    }
    return ret;
}
