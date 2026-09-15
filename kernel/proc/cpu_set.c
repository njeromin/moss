#include <proc/cpu_set.h>

bool cpu_set_contains(const struct cpu_set* set, size_t cpu_id) {
  // NULL affinity means no restriction (all CPUs allowed)
  if (set == NULL) {
    return true;
  }

  if (cpu_id >= set->num_cpus) {
    return false;
  }

  size_t mask_index = cpu_id / CPU_SET_MASK_SIZE;
  size_t bit_index = cpu_id % CPU_SET_MASK_SIZE;

  return (set->masks[mask_index] & (1ULL << bit_index)) != 0;
}
