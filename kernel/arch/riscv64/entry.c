#include "sbi.h"
#include <stddef.h>
#include <stdint.h>

static const char message[] = "Hello from the higher half!\n";

void arch_startup_core_entry(size_t hart_id, uintptr_t dtb_ptr) {
  uintptr_t phys =
      (uintptr_t)message - UINT64_C(0xffffffff80000000) + UINT64_C(0x80400000);

  struct sbi_ret_t result =
      sbi_debug_console_write(sizeof(message) - 1, phys, 0);
}
