#include "arch/tempconsole.h"
#include <core/console.h>

void console_write(const char* string) {
  arch_tempconsole_write(string);
}
