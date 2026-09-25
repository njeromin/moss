#include <sched/sched.h>

#include <arch/cpu.h>
#include <arch/memory/layout.h>
#include <arch/smp.h>
#include <arch/timer.h>
#include <arch/user.h>
#include <common/memory.h>
#include <core/console.h>
#include <core/panic.h>
#include <memory/pmm.h>
#include <proc/task.h>
#include <sched/run_queue.h>
#include <smp/smp.h>

#define _CONSOLE_LOG_SUBJECT "sched"

struct sched_worker {
  struct task* current_task;
  struct run_queue run_queue;
};

// Scheduler worker data, indexed by logical cpu id.
static struct sched_worker* scheds;

static void activate_task(struct sched_worker* sched, struct task* task,
                          size_t cpu_id) {
  task->state = TASK_STATE_RUNNING;
  task->assigned_cpu = cpu_id;
  sched->current_task = task;

  // Switch address space.
  vmm_switch_space(&task->process->memory_space, task->process->pid);

  // Program timer for the task's time slice.
  uint64_t slice_ticks = arch_timer_nanos_to_ticks(
      (uint64_t)task->time_slice_remaining * 1000000ULL);
  arch_timer_set_deadline(arch_timer_get_ticks() + slice_ticks);
  arch_timer_enable_interrupt();
}

static _Noreturn void dispatch_sched_loop(void) {
  size_t cpu_id = arch_cpu_logical_id();
  struct sched_worker* sched = &scheds[cpu_id];

  for (;;) {
    struct task* task = run_queue_pop(&sched->run_queue);

    if (task == NULL) {
      sched->current_task = NULL;
      arch_cpu_idle();
      continue;
    }

    activate_task(sched, task, cpu_id);
    arch_user_enter(task->trap_frame);
  }
}

static void cpu_start_sched(void) {
  size_t cpu_id = arch_cpu_logical_id();

  console_log(_CONSOLE_LOG_SUBJECT, "Starting worker on cpu %u.\n", cpu_id);

  dispatch_sched_loop();
}

static void init_worker(struct sched_worker* sched) {
  sched->current_task = NULL;
  run_queue_init(&sched->run_queue);
}

void sched_init(void) {
  console_log(_CONSOLE_LOG_SUBJECT, "Initialising scheduler.\n");

  // Allocate space for scheduler workers.
  size_t cpu_count = smp_cpu_count();
  size_t scheds_size = sizeof(struct sched_worker) * cpu_count;
  uintptr_t scheds_phys_addr;
  if (pmm_alloc(scheds_size, &scheds_phys_addr) != PMM_OK) {
    kpanic("Failed to allocate space for scheduler workers.");
  }
  scheds = (struct sched_worker*)arch_phys_to_virt(scheds_phys_addr);
  memory_set(scheds, 0, scheds_size);

  // Initialise each worker.
  for (size_t i = 0; i < cpu_count; i++) {
    init_worker(&scheds[i]);
  }
}

void sched_schedule(struct task* task) {
  if (task == NULL) {
    return;
  }

  size_t cpu_count = smp_cpu_count();
  size_t best_cpu_id = 0;
  size_t min_count = SIZE_MAX;
  bool found_allowed_cpu = false;

  for (size_t cpu_id = 0; cpu_id < cpu_count; ++cpu_id) {
    // Check affinity
    if (!cpu_set_contains(task->cpu_affinity, cpu_id)) {
      continue;
    }

    size_t count = scheds[cpu_id].run_queue.node_count;

    if (!found_allowed_cpu || count < min_count) {
      min_count = count;
      best_cpu_id = cpu_id;
      found_allowed_cpu = true;
    } else if (count == min_count) {
      // Prefer previous CPU or current CPU for cache locality.
      if (cpu_id == task->assigned_cpu || cpu_id == arch_cpu_logical_id()) {
        best_cpu_id = cpu_id;
      }
    }
  }

  // Fallback if affinity set had no valid CPUs enabled.
  if (!found_allowed_cpu) {
    best_cpu_id = arch_cpu_logical_id();
  }

  // Assign and enqueue task.
  task->assigned_cpu = best_cpu_id;
  task->state = TASK_STATE_READY;
  run_queue_push(&scheds[best_cpu_id].run_queue, task);

  // If assigned to another core, send an IPI to wake it from arch_cpu_idle().
  if (best_cpu_id != arch_cpu_logical_id()) {
    smp_request_reschedule(best_cpu_id);
  }
}

void sched_preempt(void) {
  size_t cpu_id = arch_cpu_logical_id();
  struct sched_worker* sched = &scheds[cpu_id];
  struct task* current = sched->current_task;

  if (current == NULL) {
    return;
  }

  // Reset time slice and re-enqueue current task.
  current->time_slice_remaining = TASK_DEFAULT_TIME_SLICE;
  current->state = TASK_STATE_READY;
  run_queue_push(&sched->run_queue, current);

  // Dequeue next task to run.
  struct task* next = run_queue_pop(&sched->run_queue);
  if (next == NULL) {
    sched->current_task = NULL;
    return;
  }

  if (next == current) {
    current->state = TASK_STATE_RUNNING;
    return;
  }

  activate_task(sched, next, cpu_id);
  arch_user_enter(next->trap_frame);
}

void sched_exit(int status) {
  (void)status;
  size_t cpu_id = arch_cpu_logical_id();
  struct sched_worker* sched = &scheds[cpu_id];
  struct task* current = sched->current_task;

  if (current != NULL) {
    current->state = TASK_STATE_DEAD;
  }

  dispatch_sched_loop();
}

void sched_start(void) {
  smp_start_secondary_cpus(cpu_start_sched);
  cpu_start_sched();
}

struct task* sched_current_task(void) {
  if (scheds == NULL) {
    return NULL;
  }
  size_t cpu_id = arch_cpu_logical_id();
  return scheds[cpu_id].current_task;
}

