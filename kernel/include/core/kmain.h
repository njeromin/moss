#pragma once

struct entry_memory_info {};

struct entry_info {
  struct entry_memory_info memory;
};

void kmain(struct entry_info entry_info);
