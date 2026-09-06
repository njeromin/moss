#pragma once

#include <stdarg.h>

__attribute__((noreturn, format(printf, 1, 2))) void kpanic(const char* format,
                                                            ...);

__attribute__((noreturn)) void kvpanic(const char* format, va_list args);
