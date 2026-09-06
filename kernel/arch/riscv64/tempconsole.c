#include <arch/tempconsole.h>

#include "sbi.h"
#include <arch/memory/layout.h>
#include <common/strings.h>

void arch_tempconsole_write(const char* string) {
  uintptr_t addr = arch_kernel_virt_to_phys((uintptr_t)string);
  sbi_debug_console_write(string_length(string), addr, 0);
}
