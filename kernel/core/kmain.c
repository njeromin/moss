#include "arch/memory/layout.h"
#include "arch/panic.h"
#include "core/memory/map.h"
#include "core/memory/pmm.h"
#include "core/memory/vmm.h"
#include "core/panic.h"
#include <arch/timer.h>
#include <core/console.h>
#include <core/kmain.h>
#include <core/memory/allocation/boot_alloc.h>

#define _CONSOLE_LOG_SUBJECT "kmain"
#define _CONSOLE_LOG_SUBJECT_MEM_INIT "kmain:memory_init"

void init_virtual_memory(const struct device_discovery_info device_discovery) {
  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT,
              "Initialising memory management.\n");

  memory_map_init(device_discovery);
  boot_alloc_init();

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

  arch_timer_init(&entry_info.device_discovery);

  init_virtual_memory(entry_info.device_discovery);

  console_log(_CONSOLE_LOG_SUBJECT, "Reached end of kmain.\n");
  arch_panic_halt();
}
