#include <proc/process.h>

#include <common/memory.h>
#include <memory/kmem_cache.h>
#include <proc/capabilities.h>

static size_t next_pid = 1;
static struct kmem_cache proc_cache;

int processes_init(void) {
  return kmem_cache_init(&proc_cache, "process", sizeof(struct process));
}

struct process* process_create(void) {
  struct process* proc = kmem_cache_zalloc(&proc_cache);
  if (proc == NULL) {
    return NULL;
  }

  proc->pid = next_pid++;
  spinlock_init(&proc->lock);
  proc->ref_count = 0;

  if (vmm_create_space(&proc->memory_space) != VMM_OK) {
    kmem_cache_free(&proc_cache, proc);
    return NULL;
  }

  spinlock_init(&proc->capabilities.lock);
  proc->capabilities.ref_count = 1;
  proc->capabilities.capacity = CAP_TABLE_INITIAL_CAPACITY;

  return proc;
}

void process_destroy(struct process* proc) {
  if (proc == NULL) {
    return;
  }

  vmm_destroy_space(&proc->memory_space);

  kmem_cache_free(&proc_cache, proc);
}
