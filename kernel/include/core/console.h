#pragma once

#include <stdarg.h>

// Supported conversions: %%, %s, %d, %u, %x, %p, %zu, and %zx.
void console_vwrite(const char* format, va_list args);
void console_write(const char* format, ...);
void console_log(const char* subject, const char* format, ...);
