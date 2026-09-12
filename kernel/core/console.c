#include <core/console.h>

#include <stddef.h>
#include <stdint.h>

#include <arch/tempconsole.h>

#define CONSOLE_BUFFER_SIZE 128

struct console_output {
  char buffer[CONSOLE_BUFFER_SIZE];
  size_t length;
};

static void stdout_flush(struct console_output* output) {
  if (output->length == 0) {
    return;
  }
  output->buffer[output->length] = '\0';
  arch_tempconsole_write(output->buffer);
  output->length = 0;
}

static void stdout_putc(struct console_output* output, char character) {
  if (output->length == CONSOLE_BUFFER_SIZE - 1) {
    stdout_flush(output);
  }
  output->buffer[output->length++] = character;
}

static void stdout_string(struct console_output* output, const char* string) {
  if (string == 0) {
    string = "(null)";
  }
  while (*string != '\0') {
    stdout_putc(output, *string++);
  }
}

static void stdout_unsigned(struct console_output* output, uintmax_t value,
                            unsigned base) {
  char digits[sizeof(uintmax_t) * 8];
  size_t length = 0;
  static const char alphabet[] = "0123456789abcdef";

  do {
    digits[length++] = alphabet[value % base];
    value /= base;
  } while (value != 0);
  while (length != 0) {
    stdout_putc(output, digits[--length]);
  }
}

static void stdout_signed(struct console_output* output, intmax_t value) {
  uintmax_t magnitude;
  if (value < 0) {
    stdout_putc(output, '-');
    magnitude = (uintmax_t)(-(value + 1)) + 1;
  } else {
    magnitude = (uintmax_t)value;
  }
  stdout_unsigned(output, magnitude, 10);
}

void console_vwrite(const char* format, va_list args) {
  struct console_output output;
  output.length = 0;

  while (*format != '\0') {
    if (*format != '%') {
      stdout_putc(&output, *format++);
      continue;
    }

    ++format;
    if (*format == 'z' && (format[1] == 'u' || format[1] == 'x')) {
      stdout_unsigned(&output, (uintmax_t)va_arg(args, size_t),
                      format[1] == 'x' ? 16 : 10);
      format += 2;
      continue;
    }
    switch (*format) {
      case '%':
        stdout_putc(&output, '%');
        break;
      case 's':
        stdout_string(&output, va_arg(args, const char*));
        break;
      case 'd':
        stdout_signed(&output, (intmax_t)va_arg(args, int));
        break;
      case 'u':
        stdout_unsigned(&output, (uintmax_t)va_arg(args, unsigned), 10);
        break;
      case 'x':
        stdout_unsigned(&output, (uintmax_t)va_arg(args, unsigned), 16);
        break;
      case 'p':
        stdout_string(&output, "0x");
        stdout_unsigned(&output, (uintmax_t)(uintptr_t)va_arg(args, void *),
                        16);
        break;
      case '\0':
        stdout_putc(&output, '%');
        stdout_flush(&output);
        return;
      default:
        stdout_putc(&output, '%');
        stdout_putc(&output, *format);
        break;
    }
    ++format;
  }

  stdout_flush(&output);
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
  console_write("[");
  console_write(subject);
  console_write("] ");
  console_vwrite(format, args);
  va_end(args);
}
