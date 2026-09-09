#pragma once

#include <arch/memory/layout.h>
#include <core/memory/vmm.h>

// Map complete discovered RAM pages, excluding all pages touched by NO_MAP.
// Ordinary reservations remain mapped. Call once on the new kernel space before
// activating it. Failure may leave partial mappings; the space must not be activated.
enum vmm_status direct_map_init(struct vmm_space* space,
                                const struct memory_map* map);
