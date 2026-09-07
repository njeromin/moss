#include <common/memory.h>

#include <stdint.h>

uintptr_t align_up(uintptr_t value, size_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

uintptr_t align_down(uintptr_t value, size_t alignment) {
  return value & ~(alignment - 1);
}
