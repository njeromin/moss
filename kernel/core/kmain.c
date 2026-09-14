#include <core/kmain.h>

#include <arch/panic.h>
#include <core/console.h>
#include <memory/init.h>
#include <proc/init.h>
#include <sched/sched.h>
#include <smp/smp.h>

#define _CONSOLE_LOG_SUBJECT "kmain"

void kmain(struct entry_info entry_info) {
  console_log(_CONSOLE_LOG_SUBJECT, "Entered kmain.\n");

  init_virtual_memory(entry_info.device_discovery);

  smp_init(&entry_info.device_discovery, entry_info.boot_cpu_hardware_id);

  init_proc_system();

  sched_init();

  smp_start_secondary_cpus();

  console_log(_CONSOLE_LOG_SUBJECT, "Reached end of kmain.\n");
  arch_panic_halt();
}
