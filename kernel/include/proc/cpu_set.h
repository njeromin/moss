#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CPU_SET_MASK_SIZE 64

struct cpu_set {
  size_t ref_count;
  size_t num_cpus;   // Number of CPUs represented
  uint64_t masks[];  // Flexible array of 64-bit bitmasks
};

bool cpu_set_contains(const struct cpu_set* set, size_t cpu_id);
