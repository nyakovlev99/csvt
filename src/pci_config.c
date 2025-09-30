#include "pci_config.h"

#include "field_32.h"

#include <stdlib.h>

typedef struct {
    void* io;
    void* field_vendor_id;
    void* field_device_id;
    void* field_command;
    void* field_command_memory;
    void* field_command_bus_control;
} pci_config_t;

int pci_config_create(void* io, uint64_t offset, void** pci_config) {
    *pci_config = calloc(sizeof(pci_config_t), 1);
    if (*pci_config == NULL) return -1;
    pci_config_t* _pci_config = (pci_config_t*)*pci_config;
    _pci_config->io = io;
    if (pci_config_field_32_create(io, offset + 0x0, 0,  0xffff,    &(_pci_config->field_vendor_id)) < 0)           return -1;
    if (pci_config_field_32_create(io, offset + 0x0, 16, 0xffff,    &(_pci_config->field_device_id)) < 0)           return -2;
    if (pci_config_field_32_create(io, offset + 0x4, 0,  0xffff,    &(_pci_config->field_command)) < 0)             return -3;
    if (pci_config_field_32_create(io, offset + 0x4, 1,  0b1,       &(_pci_config->field_command_memory)) < 0)      return -4;
    if (pci_config_field_32_create(io, offset + 0x4, 2,  0b1,       &(_pci_config->field_command_bus_control)) < 0) return -5;
    return 0;
}

int pci_config_vendor_id_read(void* pci_config, uint16_t* vendor_id) {
    pci_config_t* _pci_config = (pci_config_t*)pci_config;
    uint32_t value;
    int ret = pci_config_field_32_read(_pci_config->field_vendor_id, &value);
    if (ret < 0) return ret;
    *vendor_id = value;
    return 0;
}

int pci_config_device_id_read(void* pci_config, uint16_t* device_id) {
    pci_config_t* _pci_config = (pci_config_t*)pci_config;
    uint32_t value;
    int ret = pci_config_field_32_read(_pci_config->field_device_id, &value);
    if (ret < 0) return ret;
    *device_id = value;
    return 0;
}

int pci_config_command_read(void* pci_config, uint16_t* command) {
    pci_config_t* _pci_config = (pci_config_t*)pci_config;
    uint32_t value;
    int ret = pci_config_field_32_read(_pci_config->field_command, &value);
    if (ret < 0) return ret;
    *command = value;
    return 0;
}

int pci_config_memory_enable(void* pci_config) {
    pci_config_t* _pci_config = (pci_config_t*)pci_config;
    return pci_config_field_32_write(_pci_config->field_command_memory, 1);
}

int pci_config_memory_disable(void* pci_config) {
    pci_config_t* _pci_config = (pci_config_t*)pci_config;
    return pci_config_field_32_write(_pci_config->field_command_memory, 0);
}

int pci_config_memory_enabled(void* pci_config, uint32_t* enabled) {
    pci_config_t* _pci_config = (pci_config_t*)pci_config;
    return pci_config_field_32_read(_pci_config->field_command_memory, enabled);
}

int pci_config_bus_control_enable(void* pci_config) {
    pci_config_t* _pci_config = (pci_config_t*)pci_config;
    return pci_config_field_32_write(_pci_config->field_command_bus_control, 1);
}

int pci_config_bus_control_disable(void* pci_config) {
    pci_config_t* _pci_config = (pci_config_t*)pci_config;
    return pci_config_field_32_write(_pci_config->field_command_bus_control, 0);
}

int pci_config_bus_control_enabled(void* pci_config, uint32_t* enabled) {
    pci_config_t* _pci_config = (pci_config_t*)pci_config;
    return pci_config_field_32_read(_pci_config->field_command_bus_control, enabled);
}

// TODO: add commands to query the currently-negotiated speed and width of the link
