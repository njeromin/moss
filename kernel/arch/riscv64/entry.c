#include "sbi.h"
#include <arch/memory/layout.h>
#include <common/fdt.h>
#include <core/console.h>
#include <core/kmain.h>
#include <stddef.h>
#include <stdint.h>

void arch_startup_core_entry(size_t hart_id, uintptr_t dtb_ptr) {
  console_write("Entered C portion of arch specific init.\n");

  if (fdt_validate((const void*)dtb_ptr) == 0) {
    console_write("DTB validated ok.\n");
  }

  struct entry_info entry_info = {
      .memory = {},
  };

  kmain(entry_info);
}
