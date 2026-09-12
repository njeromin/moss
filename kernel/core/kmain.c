#include <core/kmain.h>

#include <arch/panic.h>
#include <arch/timer.h>
#include <core/console.h>
#include <memory/init.h>

#define _CONSOLE_LOG_SUBJECT "kmain"

void kmain(struct entry_info entry_info) {
  console_log(_CONSOLE_LOG_SUBJECT, "Entered kmain.\n");

  arch_timer_init(&entry_info.device_discovery);

  init_virtual_memory(entry_info.device_discovery);

  console_log(_CONSOLE_LOG_SUBJECT, "Reached end of kmain.\n");
  arch_panic_halt();
}
