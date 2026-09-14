#pragma once

#include <stddef.h>
#include <memory/vmm.h>
#include <sync/spinlock.h>
#include <proc/capabilities.h>

struct process {
  size_t pid;
  struct spinlock lock;

  struct vmm_space memory_space;
  struct capability_table capabilities;

  size_t ref_count;
};

int processes_init(void);

struct process* process_create(void);
void process_destroy(struct process* proc);
