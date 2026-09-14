#include <core/kmain.h>

#include <stddef.h>
#include <stdint.h>
#include <arch/cpu.h>
#include <arch/memory/layout.h>
#include <arch/timer.h>
#include <common/fdt.h>
#include <core/console.h>
#include <core/panic.h>

#define _CONSOLE_LOG_SUBJECT "arch_entry"

void arch_startup_core_entry(cpu_hardware_id_t hart_id, uintptr_t dtb_ptr,
                             size_t dtb_size) {
  console_log(_CONSOLE_LOG_SUBJECT,
              "Entered C portion of arch specific init.\n");

  arch_cpu_init(hart_id);

  // The bootstrap identity-maps the DTB and supplies its readable bound.
  if (fdt_check_full((const void*)dtb_ptr, dtb_size) != 0) {
    kpanic("Failed to validate DTB.\n");
  }

  console_log(_CONSOLE_LOG_SUBJECT, "Verified DTB.\n");

  struct entry_info entry_info = {
      .boot_cpu_hardware_id = hart_id,
      .device_discovery =
          {
              .dtb_phys = dtb_ptr,
              .dtb_size = dtb_size,

              .acpi_rsdp_phys = 0,
          },
  };

  arch_timer_init(&entry_info.device_discovery);

  kmain(entry_info);
}
