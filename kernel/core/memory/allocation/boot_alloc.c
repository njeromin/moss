#include <core/memory/allocation/boot_alloc.h>

#include <common/memory.h>

void* boot_alloc(struct boot_allocator* allocator, size_t size,
                 size_t alignment) {
  uintptr_t alloc_curr;
  uintptr_t next;

  if (allocator == 0 || size == 0 || alignment == 0 ||
      (alignment & (alignment - 1)) != 0 ||
      allocator->current > UINTPTR_MAX - (alignment - 1)) {
    return 0;
  }

  alloc_curr = align_up(allocator->current, alignment);
  if (alloc_curr > allocator->end || size > allocator->end - alloc_curr) {
    return 0;
  }

  next = alloc_curr + size;
  allocator->current = next;

  return (void*)alloc_curr;
}
