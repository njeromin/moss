#include "arch/memory/layout.h"
#include "core/memory/regions.h"
#include "core/panic.h"
#include <core/kmain.h>

#include <core/console.h>
#include <core/memory/pmm.h>

#define _CONSOLE_LOG_SUBJECT "kmain"
#define _CONSOLE_LOG_SUBJECT_MEM_INIT "kmain:memory_init"

#define MAX_MEMORY_REGIONS 128

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

  (void)pmm_init(&map);
}

void kmain(struct entry_info entry_info) {
  console_log(_CONSOLE_LOG_SUBJECT, "Entered kmain.\n");

  init_virtual_memory(entry_info.device_discovery);

  kpanic("Reached end of kmain");
}
