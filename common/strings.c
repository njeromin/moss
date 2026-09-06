#include <common/strings.h>

size_t string_length(const char* string) {
  size_t i = 0;
  while (string[i] != '\0') {
    i++;
  }
  return i;
}

int string_length_bounded(const char *string, size_t max_size,
                          size_t *length) {
  size_t i;
  if (string == 0 || length == 0) return -1;
  for (i = 0; i < max_size; ++i) {
    if (string[i] == '\0') {
      *length = i + 1;
      return 0;
    }
  }
  return -1;
}
