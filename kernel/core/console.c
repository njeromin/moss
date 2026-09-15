#include <core/console.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <arch/tempconsole.h>

#include <sync/spinlock.h>

#define CONSOLE_BUFFER_SIZE 1024

static struct spinlock console_lock = SPINLOCK_INIT;
static char console_buffer[CONSOLE_BUFFER_SIZE];
static size_t console_buffer_len;

static void stdout_flush(void) {
  if (console_buffer_len == 0) {
    return;
  }
  console_buffer[console_buffer_len] = '\0';
  arch_tempconsole_write(console_buffer);
  console_buffer_len = 0;
}

static void stdout_putc(char character) {
  if (console_buffer_len >= CONSOLE_BUFFER_SIZE - 1) {
    stdout_flush();
  }
  console_buffer[console_buffer_len++] = character;
  if (character == '\n') {
    stdout_flush();
  }
}

static void stdout_string(const char* string) {
  if (string == NULL) {
    string = "(null)";
  }
  while (*string != '\0') {
    stdout_putc(*string++);
  }
}

static void stdout_unsigned(uintmax_t value, unsigned base) {
  char digits[sizeof(uintmax_t) * 8];
  size_t length = 0;
  static const char alphabet[] = "0123456789abcdef";

  do {
    digits[length++] = alphabet[value % base];
    value /= base;
  } while (value != 0);
  while (length != 0) {
    stdout_putc(digits[--length]);
  }
}

static void stdout_signed(intmax_t value) {
  uintmax_t magnitude;
  if (value < 0) {
    stdout_putc('-');
    magnitude = (uintmax_t)(-(value + 1)) + 1;
  } else {
    magnitude = (uintmax_t)value;
  }
  stdout_unsigned(magnitude, 10);
}

static void stdout_vformat(const char* format, va_list args) {
  while (*format != '\0') {
    if (*format != '%') {
      stdout_putc(*format++);
      continue;
    }

    ++format;
    if (*format == 'z' && (format[1] == 'u' || format[1] == 'x')) {
      stdout_unsigned((uintmax_t)va_arg(args, size_t),
                      format[1] == 'x' ? 16 : 10);
      format += 2;
      continue;
    }
    switch (*format) {
      case '%':
        {
          stdout_putc('%');
          break;
        }
      case 's':
        {
          stdout_string(va_arg(args, const char*));
          break;
        }
      case 'd':
        {
          stdout_signed((intmax_t)va_arg(args, int));
          break;
        }
      case 'u':
        {
          stdout_unsigned((uintmax_t)va_arg(args, unsigned), 10);
          break;
        }
      case 'x':
        {
          stdout_unsigned((uintmax_t)va_arg(args, unsigned), 16);
          break;
        }
      case 'p':
        {
          stdout_string("0x");
          stdout_unsigned((uintmax_t)(uintptr_t)va_arg(args, void*), 16);
          break;
        }
      case '\0':
        {
          stdout_putc('%');
          return;
        }
      default:
        {
          stdout_putc('%');
          stdout_putc(*format);
          break;
        }
    }
    ++format;
  }
}

void console_flush(void) {
  bool flags = spin_lock_irqsave(&console_lock);
  stdout_flush();
  spin_unlock_irqrestore(&console_lock, flags);
}

void console_vwrite(const char* format, va_list args) {
  bool flags = spin_lock_irqsave(&console_lock);
  stdout_vformat(format, args);
  spin_unlock_irqrestore(&console_lock, flags);
}

void console_write(const char* format, ...) {
  va_list args;
  va_start(args, format);
  console_vwrite(format, args);
  va_end(args);
}

void console_log(const char* subject, const char* format, ...) {
  va_list args;
  va_start(args, format);
  bool flags = spin_lock_irqsave(&console_lock);
  stdout_putc('[');
  stdout_string(subject);
  stdout_string("] ");
  stdout_vformat(format, args);
  spin_unlock_irqrestore(&console_lock, flags);
  va_end(args);
}
