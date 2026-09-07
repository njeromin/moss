#pragma once

#include <core/kmain.h>
#include <core/memory/regions.h>
#include <stdint.h>

// Translate an address in the fixed higher-half kernel mapping. The selected
// architecture supplies the implementation; this does not walk page tables.
uintptr_t arch_kernel_virt_to_phys(uintptr_t virt);

// Writable identity-mapped range usable by the early allocator.
struct memory_region arch_boot_alloc_window(void);

// Physical bounds emitted by the selected architecture's linker script.
extern const uintptr_t kernel_physical_start;
extern const uintptr_t kernel_physical_end;

enum arch_memory_map_error {
  ARCH_MEMORY_MAP_OK = 0,
  ARCH_MEMORY_MAP_INVALID = -1,
  ARCH_MEMORY_MAP_NO_SPACE = -2,
};

struct memory_map {
  struct memory_region* available;
  size_t available_capacity;
  size_t available_count;

  struct memory_region* reserved;
  size_t reserved_capacity;
  size_t reserved_count;

  // The highest physical address found.
  size_t max_phys_addr;
  // The smallest physical address found.
  size_t min_phys_addr;
};

int arch_get_memory_map(
    const struct device_discovery_info device_discovery_info,
    struct memory_map* memory_map);
