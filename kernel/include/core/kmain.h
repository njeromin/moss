#pragma once

#include <stddef.h>
#include <stdint.h>

struct device_discovery_info {
  // Physical address of the DTB.
  uintptr_t dtb;
};

struct entry_info {
  struct device_discovery_info device_discovery;
  // struct entry_memory_info memory;
};

void kmain(struct entry_info entry_info);
