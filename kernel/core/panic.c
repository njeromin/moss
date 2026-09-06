#include <core/panic.h>

#include <arch/panic.h>
#include <core/console.h>

void kvpanic(const char* format, va_list args) {
  console_write("\n[KERNEL PANIC]\n");
  console_vwrite(format, args);
  console_write("\n");

  arch_panic_halt();
  __builtin_unreachable();
}

void kpanic(const char* format, ...) {
  va_list args;

  va_start(args, format);
  kvpanic(format, args);
  va_end(args);

  __builtin_unreachable();
}
