#include <proc/process.h>

#include <common/memory.h>
#include <dma/dma.h>
#include <ipc/channel.h>
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

  for (size_t i = 0; i < proc->capabilities.capacity; i++) {
    if (proc->capabilities.slots[i].type == CAP_TYPE_CHANNEL &&
        proc->capabilities.slots[i].object != NULL) {
      channel_endpoint_close(
          (struct channel_endpoint*)proc->capabilities.slots[i].object);
      proc->capabilities.slots[i].type = CAP_TYPE_NONE;
      proc->capabilities.slots[i].object = NULL;
    } else if (proc->capabilities.slots[i].type == CAP_TYPE_DMA_BUFFER &&
               proc->capabilities.slots[i].object != NULL) {
      struct capability* cap = &proc->capabilities.slots[i];
      struct dma_buffer* buffer = (struct dma_buffer*)cap->object;
      if (vmm_unmap(&proc->memory_space, (virt_addr_t)cap->badge,
                    dma_buffer_allocation_size(buffer)) == VMM_OK) {
        dma_free_coherent(buffer);
      }
      cap->type = CAP_TYPE_NONE;
      cap->object = NULL;
    }
  }

  vmm_destroy_space(&proc->memory_space);

  kmem_cache_free(&proc_cache, proc);
}
