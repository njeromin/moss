#include "arch/memory/layout.h"
#include "core/memory/regions.h"
#include "core/panic.h"
#include <core/kmain.h>

#include <core/console.h>
#include <core/memory/allocation/boot_alloc.h>
#include <core/memory/pmm.h>

#define _CONSOLE_LOG_SUBJECT "kmain"
#define _CONSOLE_LOG_SUBJECT_MEM_INIT "kmain:memory_init"

#define MAX_MEMORY_REGIONS 64

void init_virtual_memory(const struct device_discovery_info device_discovery) {
  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT, "Initialising virtual memory.\n");

  struct memory_region available[MAX_MEMORY_REGIONS];
  struct memory_region reserved[MAX_MEMORY_REGIONS];

  struct memory_map map = {
      .available = available,
      .available_capacity = MAX_MEMORY_REGIONS,
      .reserved = reserved,
      .reserved_capacity = MAX_MEMORY_REGIONS,
  };
  int mem_map_init_res = ARCH_MEMORY_MAP_OK;
  if ((mem_map_init_res = arch_get_memory_map(device_discovery, &map)) !=
      ARCH_MEMORY_MAP_OK) {
    switch (mem_map_init_res) {
      case ARCH_MEMORY_MAP_INVALID:
        kpanic("Encountered invalid map while getting memory map.\n");
        break;
      case ARCH_MEMORY_MAP_NO_SPACE:
        kpanic("No space while getting memory map.\n");
        break;
      default:
        kpanic("Unexpected error while getting memory map.\n");
        break;
    }
  }
  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT,
              "Found %d available memory regions.\n", map.available_count);

  for (size_t i = 0; i < map.available_count; i++) {
    struct memory_region region = map.available[i];
    console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT, "-> 0x%x (%d bytes)\n",
                region.offset, region.size);
  }

  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT,
              "Found %d reserved memory regions.\n", map.reserved_count);

  for (size_t i = 0; i < map.reserved_count; i++) {
    struct memory_region region = map.reserved[i];
    console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT, "-> 0x%x (%d bytes)\n",
                region.offset, region.size);
  }

  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT, "Lowest address found:  0x%x.\n",
              map.min_phys_addr);
  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT, "Highest address found: 0x%x.\n",
              map.max_phys_addr);

  struct boot_allocator boot_allocator = {
      .current = (uintptr_t)boot_allocator_start,
      .end = (uintptr_t)boot_allocator_end,
  };

  if (pmm_init(&map, &boot_allocator) != 0) {
    kpanic("Failed to initialise the PMM.");
  }
}

void kmain(struct entry_info entry_info) {
  console_log(_CONSOLE_LOG_SUBJECT, "Entered kmain.\n");

  init_virtual_memory(entry_info.device_discovery);

  kpanic("Reached end of kmain");
}
