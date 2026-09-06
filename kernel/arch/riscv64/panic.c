#include <arch/panic.h>

__attribute__((noreturn)) void arch_panic_halt(void) {
  for (;;) {
    __asm__ volatile("wfi");
  }
}
