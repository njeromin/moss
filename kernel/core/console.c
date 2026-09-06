#include "arch/tempconsole.h"
#include <core/console.h>

void console_write(const char* string) {
  arch_tempconsole_write(string);
}

void console_log(const char* subject, const char* message) {
  arch_tempconsole_write("[");
  arch_tempconsole_write(subject);
  arch_tempconsole_write("] ");
  arch_tempconsole_write(message);
}
