#pragma once

#include <stddef.h>
#include <stdint.h>

uintptr_t align_up(uintptr_t value, size_t alignment);
uintptr_t align_down(uintptr_t value, size_t alignment);

void memory_set(void* addr, size_t value, size_t size);
void memory_copy(void* addr, const void* src_addr, size_t size);
