#pragma once

#include <core/kmain.h>
#include <core/memory/regions.h>
#include <stdint.h>

// Translate an address in the fixed higher-half kernel mapping. The selected
// architecture supplies the implementation; this does not walk page tables.
uintptr_t arch_kernel_virt_to_phys(uintptr_t virt);

int arch_get_memory_map(
    const struct device_discovery_info device_discovery_info,
    struct memory_region* regions, size_t capacity, size_t* count);
