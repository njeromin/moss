#include <memory/init.h>

#include <core/console.h>
#include <core/panic.h>
#include <memory/boot_alloc.h>
#include <memory/memory_map.h>
#include <memory/pmm.h>
#include <memory/vmm.h>

#define _CONSOLE_LOG_SUBJECT_MEM_INIT "memory_init"

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

  if (vmm_init() != VMM_OK) {
    kpanic("Failed to initialise virtual memory manager.");
  }

  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT,
              "Initialised virtual memory manager.\n");
}
