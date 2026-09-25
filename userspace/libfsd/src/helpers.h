#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <string.h>

static inline bool fsd_path_next_component(const char* path,
                                           char* out_component,
                                           size_t max_component_len,
                                           const char** out_remaining) {
  if (path == NULL || out_component == NULL || max_component_len == 0) {
    return false;
  }

  // Skip leading slashes
  while (*path == '/') {
    path++;
  }

  if (*path == '\0') {
    return false;
  }

  size_t idx = 0;
  while (*path != '\0' && *path != '/') {
    if (idx + 1 < max_component_len) {
      out_component[idx] = *path;
      idx++;
    }
    path++;
  }
  out_component[idx] = '\0';

  // Skip consecutive trailing slashes
  while (*path == '/') {
    path++;
  }

  if (out_remaining != NULL) {
    if (*path == '\0') {
      *out_remaining = NULL;
    } else {
      *out_remaining = path;
    }
  }

  return true;
}
