#include "arch/memory/layout.h"
#include "core/memory/regions.h"
#include <core/kmain.h>

#include <core/console.h>
#include <core/memory/pmm.h>

#define _CONSOLE_LOG_SUBJECT "kmain"

#define MAX_BOOT_MEMORY_REGIONS 128

void kmain(struct entry_info entry_info) {
  console_log(_CONSOLE_LOG_SUBJECT, "Entered kmain.\n");

  struct memory_region memory_regions[MAX_BOOT_MEMORY_REGIONS];
  size_t memory_regions_count = 0;
  (void)arch_get_memory_map(entry_info.device_discovery, memory_regions, 0,
                            &memory_regions_count);
  (void)pmm_init();
}
