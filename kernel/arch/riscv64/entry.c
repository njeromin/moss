#include <core/kmain.h>

#include "sbi.h"
#include <arch/memory/layout.h>
#include <common/fdt.h>
#include <core/console.h>
#include <stddef.h>
#include <stdint.h>

#define _CONSOLE_LOG_SUBJECT "arch_entry"

void arch_startup_core_entry(size_t hart_id, uintptr_t dtb_ptr) {
  console_log(_CONSOLE_LOG_SUBJECT,
              "Entered C portion of arch specific init.\n");

  // The temporary identity mapping covers 0x80000000..0xc0000000.
  size_t dtb_size = (uintptr_t)0xc0000000ULL - dtb_ptr;
  if (dtb_ptr < (uintptr_t)0x80000000ULL ||
      dtb_ptr >= (uintptr_t)0xc0000000ULL) {
    dtb_size = 0;
  }
  if (fdt_check_full((const void*)dtb_ptr, dtb_size) != 0) {
    console_log(_CONSOLE_LOG_SUBJECT, "DTB failed validation.\n");
  }

  struct entry_info entry_info = {
      .device_discovery =
          {
              .dtb = dtb_ptr,
          },
  };

  kmain(entry_info);
}
