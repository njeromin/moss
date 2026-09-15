#pragma once

#include <proc/task.h>

struct trap_frame;

void sched_init(void);

void sched_schedule(struct task* task);

void sched_preempt(struct trap_frame* tf);

void sched_start(void);
