#pragma once

#include <arch/memory/layout.h>
#include <core/kmain.h>
#include <memory/regions.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Initialise the global memory map from hardware/device discovery.
 * Populates static .bss storage via arch_get_memory_map and logs regions.
 */
int memory_map_init(const struct device_discovery_info device_discovery);

/*
 * Returns a read-only pointer to the master kernel memory map.
 */
const struct memory_map* memory_map_get(void);

/*
 * Append a newly reserved physical range to the master memory map.
 * Returns 0 on success, or -1 if the reservation table is at capacity.
 */
int memory_map_add_reserved(struct memory_region region);
