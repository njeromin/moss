#include "arch/memory/layout.h"
#include "arch/panic.h"
#include "core/memory/regions.h"
#include "core/memory/vmm.h"
#include "core/panic.h"
#include <core/kmain.h>

#include <core/console.h>
#include <core/memory/allocation/boot_alloc.h>
#include <core/memory/pmm.h>

#define _CONSOLE_LOG_SUBJECT "kmain"
#define _CONSOLE_LOG_SUBJECT_MEM_INIT "kmain:memory_init"

void init_virtual_memory(const struct device_discovery_info device_discovery) {
  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT,
              "Initialising memory management.\n");

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
              "Found %zu available memory regions.\n", map.available_count);

  for (size_t i = 0; i < map.available_count; i++) {
    struct memory_region region = map.available[i];
    console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT, "-> %p (%zu bytes)\n",
                (void*)region.offset, region.size);
  }

  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT,
              "Found %zu reserved memory regions.\n", map.reserved_count);

  for (size_t i = 0; i < map.reserved_count; i++) {
    struct memory_region region = map.reserved[i];
    console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT, "-> %p (%zu bytes)\n",
                (void*)region.offset, region.size);
  }

  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT, "Lowest address found:  %p.\n",
              (void*)map.min_phys_addr);
  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT, "Highest address found: %p.\n",
              (void*)map.max_phys_addr);

  struct memory_region window = arch_boot_alloc_window();
  boot_alloc_init(&map, window.offset, window.offset + window.size);

  int pmm_err;
  if ((pmm_err = pmm_init()) != PMM_OK) {
    switch (pmm_err) {
      case PMM_ERROR_INVALID_INPUT:
        kpanic("Invalid input while initialising PMM.");
        break;
      case PMM_ERROR_CANT_EMIT_REGION:
        kpanic("Failed while emitting regions during PMM initialisation.");
        break;
      default:
        kpanic("Unknown error while initialising PMM.");
        break;
    }
  }

  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT,
              "Initialised physical memory manager.\n");

  if (vmm_init() != 0) {
    kpanic("Failed to initialise virtual memory manager.");
  }

  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT,
              "Initialised virtual memory manager.\n");
}

void kmain(struct entry_info entry_info) {
  console_log(_CONSOLE_LOG_SUBJECT, "Entered kmain.\n");

  init_virtual_memory(entry_info.device_discovery);

  console_log(_CONSOLE_LOG_SUBJECT, "Reached end of kmain.\n");
  arch_panic_halt();
}
