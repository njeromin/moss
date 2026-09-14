#include <arch/tempconsole.h>

#include <arch/memory/layout.h>
#include <common/strings.h>
#include "sbi.h"

void arch_tempconsole_write(const char* string) {
  uintptr_t virt = (uintptr_t)string;
  uintptr_t addr =
      (virt >= (uintptr_t)text_start && virt < (uintptr_t)kernel_end)
          ? arch_kernel_virt_to_phys(virt)
          : arch_virt_to_phys(virt);
  sbi_debug_console_write(string_length(string), addr, 0);
}
