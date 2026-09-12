#include <core/kmain.h>

#include "sbi.h"
#include <arch/timer.h>
#include <core/console.h>
#include <memory/init.h>
#include <core/panic.h>

void paging_test(void);

void kmain(struct entry_info entry_info) {
  arch_timer_init(&entry_info.device_discovery);
  init_virtual_memory(entry_info.device_discovery);
  paging_test();
  console_log("paging_test", "All paging tests passed.\n");
  sbi_system_reset(SBI_SRST_RESET_TYPE_SHUTDOWN, SBI_SRST_RESET_REASON_NONE);
  kpanic("Test image failed to shut down");
}
