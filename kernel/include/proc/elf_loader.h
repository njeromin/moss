#pragma once

#include <common/elf.h>
#include <proc/process.h>
#include <stddef.h>
#include <stdint.h>

struct elf_load_result {
  uintptr_t entry_point;
};

enum elf_loader_status {
  ELF_LOAD_OK = 0,
  ELF_LOAD_ERR_INVALID_HEADER = -1,
  ELF_LOAD_ERR_UNSUPPORTED = -2,
  ELF_LOAD_ERR_CORRUPT = -3,
  ELF_LOAD_ERR_NOMEM = -4,
  ELF_LOAD_ERR_MAP_FAILED = -5,
};

// Bootstrap ELF loader for initial user programmes (such as initd).
// Parses static PT_LOAD segments, allocates physical frames, maps them into
// the process address space, zeroes BSS regions, and returns the entry point.
int elf_load_bootstrap(struct process* proc, const void* elf_data,
                       size_t elf_size, struct elf_load_result* out_result);
