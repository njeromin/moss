#pragma once

#include <proc/task.h>

struct trap_frame;

void sched_init(void);

void sched_schedule(struct task* task);

void sched_preempt(void);

void sched_exit(int status);

void sched_block_current(enum task_state block_state);

void sched_start(void);

struct task* sched_current_task(void);
