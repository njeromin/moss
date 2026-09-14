#include <core/kmain.h>

#include <arch/panic.h>
#include <core/console.h>
#include <memory/init.h>
#include <proc/init.h>

#define _CONSOLE_LOG_SUBJECT "kmain"

void kmain(struct entry_info entry_info) {
  console_log(_CONSOLE_LOG_SUBJECT, "Entered kmain.\n");

  init_virtual_memory(entry_info.device_discovery);

  init_proc_system();

  console_log(_CONSOLE_LOG_SUBJECT, "Reached end of kmain.\n");
  arch_panic_halt();
}
