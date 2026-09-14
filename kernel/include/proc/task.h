#pragma once

#include <arch/memory/paging.h>
#include <asm/trap_layout.h>
#include <ipc/ipc.h>
#include <proc/capabilities.h>
#include <proc/process.h>

#define TASK_DEFAULT_PRIORITY 10
#define TASK_DEFAULT_TIME_SLICE 10

enum task_state {
  TASK_STATE_READY,
  TASK_STATE_RUNNING,
  TASK_STATE_BLOCKED_IPC,
  TASK_STATE_BLOCKED_TIME,
  TASK_STATE_DEAD,
};

struct task {
  struct trap_frame trap_frame;

  size_t tid;
  struct process* process;

  enum task_state state;
  uint8_t priority;
  uint32_t time_slice_remaining;
  size_t assigned_cpu;

  struct task* next;
  struct task* prev;

  struct ipc_endpoint* waiting_endpoint;
  struct task* reply_target;

  // User stack address and size for cleanup.
  virt_addr_t user_stack_base;
  size_t user_stack_size;
};

int tasks_init(void);

struct task* task_create(struct process* proc, uintptr_t entry_point,
                         uintptr_t arg0);

void task_destroy(struct task* task);
