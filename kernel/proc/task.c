#include <proc/task.h>

#include <arch/memory/layout.h>
#include <arch/memory/paging.h>
#include <arch/user.h>
#include <common/memory.h>
#include <memory/kmem_cache.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <sync/spinlock.h>

#define USER_STACK_SIZE (64 * 1024)
#define USER_STACK_TOP 0x0000003fffff0000ULL

#define KERNEL_STACK_SIZE (16 * 1024)

static size_t next_tid = 1;
static struct kmem_cache task_cache;

int tasks_init(void) {
  return kmem_cache_init(&task_cache, "task", sizeof(struct task));
}

struct task* task_create(struct process* proc, uintptr_t entry_point,
                         uintptr_t arg0) {
  if (proc == NULL || entry_point == 0) {
    return NULL;
  }

  struct task* task = kmem_cache_zalloc(&task_cache);
  if (task == NULL) {
    return NULL;
  }

  task->tid = next_tid++;
  task->process = proc;

  bool irq = spin_lock_irqsave(&proc->lock);
  proc->ref_count++;
  spin_unlock_irqrestore(&proc->lock, irq);

  size_t kernel_stack_frames = KERNEL_STACK_SIZE / PMM_FRAME_SIZE;
  uintptr_t kernel_stack_phys;

  if (pmm_alloc(kernel_stack_frames, &kernel_stack_phys) != PMM_OK) {
    bool rollback_irq = spin_lock_irqsave(&proc->lock);
    proc->ref_count--;
    spin_unlock_irqrestore(&proc->lock, rollback_irq);

    kmem_cache_free(&task_cache, task);
    return NULL;
  }

  task->kernel_stack_base = arch_phys_to_virt(kernel_stack_phys);
  task->kernel_stack_size = KERNEL_STACK_SIZE;
  memory_set((void*)task->kernel_stack_base, 0, KERNEL_STACK_SIZE);

  size_t user_stack_slot = task->tid - 1;
  virt_addr_t user_stack_top =
      USER_STACK_TOP - (user_stack_slot * (USER_STACK_SIZE + PMM_FRAME_SIZE));
  virt_addr_t user_stack_base = user_stack_top - USER_STACK_SIZE;

  task->user_stack_base = user_stack_base;
  task->user_stack_size = USER_STACK_SIZE;

  size_t user_stack_frames = USER_STACK_SIZE / PMM_FRAME_SIZE;
  uintptr_t user_stack_phys;

  if (pmm_alloc(user_stack_frames, &user_stack_phys) != PMM_OK) {
    pmm_free(kernel_stack_phys, kernel_stack_frames);

    bool rollback_irq = spin_lock_irqsave(&proc->lock);
    proc->ref_count--;
    spin_unlock_irqrestore(&proc->lock, rollback_irq);

    kmem_cache_free(&task_cache, task);
    return NULL;
  }

  memory_set((void*)arch_phys_to_virt(user_stack_phys), 0, USER_STACK_SIZE);

  if (vmm_map(&proc->memory_space, user_stack_base, user_stack_phys,
              USER_STACK_SIZE, PAGE_USER | PAGE_READ | PAGE_WRITE) != VMM_OK) {
    pmm_free(user_stack_phys, user_stack_frames);
    pmm_free(kernel_stack_phys, kernel_stack_frames);

    bool rollback_irq = spin_lock_irqsave(&proc->lock);
    proc->ref_count--;
    spin_unlock_irqrestore(&proc->lock, rollback_irq);

    kmem_cache_free(&task_cache, task);
    return NULL;
  }

  uintptr_t kstack_top = task->kernel_stack_base + task->kernel_stack_size;
  task->trap_frame =
      (struct trap_frame*)(kstack_top - sizeof(struct trap_frame));
  arch_user_context_init(task->trap_frame, entry_point, user_stack_top, arg0);

  task->state = TASK_STATE_READY;
  task->priority = TASK_DEFAULT_PRIORITY;
  task->time_slice_remaining = TASK_DEFAULT_TIME_SLICE;
  task->assigned_cpu = 0;
  task->cpu_affinity = NULL;

  task->waiting_endpoint = NULL;
  task->reply_target = NULL;

  return task;
}

void task_destroy(struct task* task) {
  if (task == NULL) {
    return;
  }

  if (task->user_stack_base != 0 && task->user_stack_size != 0 &&
      task->process != NULL) {
    phys_addr_t stack_phys = 0;
    if (vmm_translate(&task->process->memory_space, task->user_stack_base,
                      &stack_phys) == VMM_OK) {
      vmm_unmap(&task->process->memory_space, task->user_stack_base,
                task->user_stack_size);
      pmm_free(stack_phys, task->user_stack_size / PMM_FRAME_SIZE);
    }
  }

  if (task->kernel_stack_base != 0 && task->kernel_stack_size != 0) {
    pmm_free(arch_virt_to_phys(task->kernel_stack_base),
             task->kernel_stack_size / PMM_FRAME_SIZE);
  }

  if (task->process != NULL) {
    bool irq = spin_lock_irqsave(&task->process->lock);
    if (task->process->ref_count > 0) {
      task->process->ref_count--;
    }
    spin_unlock_irqrestore(&task->process->lock, irq);
  }

  kmem_cache_free(&task_cache, task);
}
