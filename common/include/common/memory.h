#pragma once

#include <stddef.h>
#include <stdint.h>

uintptr_t align_up(uintptr_t value, size_t alignment);
uintptr_t align_down(uintptr_t value, size_t alignment);
