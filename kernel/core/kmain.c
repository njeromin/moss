#include "arch/memory/layout.h"
#include <core/kmain.h>

#include <core/console.h>
#include <core/memory/pmm.h>

#define _CONSOLE_LOG_SUBJECT

void kmain(struct entry_info entry_info) {
  console_log("kmain", "Entered kmain.\n");

  (void)arch_get_memory_map((void*)entry_info.device_discovery.dtb, NULL, 0,
                            NULL);
  (void)pmm_init();
}
