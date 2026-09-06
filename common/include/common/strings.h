#pragma once

#include <stddef.h>

size_t string_length(const char* string);
int string_length_bounded(const char *string, size_t max_size,
                          size_t *length);
