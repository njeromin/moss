#include "include/strings.h"

size_t string_length(const char* string) {
  size_t i = 0;
  while (string[i] != '\0') {
    i++;
  }
  return i;
}
