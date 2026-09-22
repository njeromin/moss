#pragma once

#include <core/kmain.h>
#include <stddef.h>
#include <stdint.h>

int get_initramfs_addr(const struct device_discovery_info* discovery,
                       uintptr_t* out_addr, size_t* out_size);

int initramfs_load(const struct device_discovery_info* device_discovery_info);

int initramfs_find_file(const char* name, const void** out_data,
                        size_t* out_size);
