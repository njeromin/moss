#include <core/kmain.h>

#include <arch/memory/layout.h>
#include <common/fdt.h>
#include <core/console.h>
#include <core/panic.h>
#include <stddef.h>
#include <stdint.h>

#define _CONSOLE_LOG_SUBJECT "arch_entry"

void arch_startup_core_entry(size_t hart_id, uintptr_t dtb_ptr,
                             size_t dtb_size) {
  (void)hart_id;
  console_log(_CONSOLE_LOG_SUBJECT,
              "Entered C portion of arch specific init.\n");

  // The bootstrap identity-maps the DTB and supplies its readable bound.
  if (fdt_check_full((const void*)dtb_ptr, dtb_size) != 0) {
    kpanic("Failed to validate DTB.\n");
  }

  console_log(_CONSOLE_LOG_SUBJECT, "Verified DTB.\n");

  struct entry_info entry_info = {
      .device_discovery =
          {
              .dtb = dtb_ptr,
          },
  };

  kmain(entry_info);
}
