#include <dma/config.h>

#include <arch/memory/layout.h>
#include <memory/kmem_cache.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <sync/spinlock.h>

struct dma_ctx {
  dma_addr_t address_mask;
  struct dma_range* ranges;
  uintptr_t ranges_phys;
  size_t range_count;
  size_t range_frames;
  size_t live_objects;
  bool coherent;
  struct spinlock lock;
};

struct dma_buffer {
  struct dma_ctx* ctx;
  void* cpu_addr;
  dma_addr_t device_addr;
  uintptr_t phys;
  size_t size;
  size_t frames;
};

struct dma_mapping {
  struct dma_ctx* ctx;
  dma_addr_t device_addr;
  size_t size;
};

static struct spinlock cache_init_lock = SPINLOCK_INIT;
static bool caches_ready;
static struct kmem_cache ctx_cache;
static struct kmem_cache buffer_cache;
static struct kmem_cache mapping_cache;

static bool is_power_of_two(size_t value) {
  return value != 0 && (value & (value - 1)) == 0;
}

static bool ranges_valid(const struct dma_range* ranges, size_t count) {
  if (ranges == NULL || count == 0 ||
      count > SIZE_MAX / sizeof(struct dma_range)) {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    const struct dma_range* range = &ranges[i];
    if (range->size == 0 || range->size > UINTPTR_MAX - range->phys_start ||
        range->size > UINT64_MAX - range->device_start) {
      return false;
    }
    for (size_t j = 0; j < i; ++j) {
      const struct dma_range* other = &ranges[j];
      if ((range->phys_start < other->phys_start + other->size &&
           other->phys_start < range->phys_start + range->size) ||
          (range->device_start < other->device_start + other->size &&
           other->device_start < range->device_start + range->size)) {
        return false;
      }
    }
  }
  return true;
}

static bool init_caches(void) {
  bool irq = spin_lock_irqsave(&cache_init_lock);
  if (!caches_ready) {
    if (kmem_cache_init(&ctx_cache, "dma_ctx", sizeof(struct dma_ctx)) != 0 ||
        kmem_cache_init(&buffer_cache, "dma_buffer",
                        sizeof(struct dma_buffer)) != 0 ||
        kmem_cache_init(&mapping_cache, "dma_mapping",
                        sizeof(struct dma_mapping)) != 0) {
      spin_unlock_irqrestore(&cache_init_lock, irq);
      return false;
    }
    caches_ready = true;
  }
  spin_unlock_irqrestore(&cache_init_lock, irq);
  return true;
}

static void ctx_add_object(struct dma_ctx* ctx) {
  bool irq = spin_lock_irqsave(&ctx->lock);
  ++ctx->live_objects;
  spin_unlock_irqrestore(&ctx->lock, irq);
}

static void ctx_remove_object(struct dma_ctx* ctx) {
  bool irq = spin_lock_irqsave(&ctx->lock);
  --ctx->live_objects;
  spin_unlock_irqrestore(&ctx->lock, irq);
}

static bool direct_address(const struct dma_ctx* ctx, uintptr_t phys,
                           size_t size, dma_addr_t* out) {
  for (size_t i = 0; i < ctx->range_count; ++i) {
    const struct dma_range* range = &ctx->ranges[i];
    if (phys < range->phys_start || size > range->size ||
        phys - range->phys_start > range->size - size) {
      continue;
    }
    dma_addr_t addr = range->device_start + phys - range->phys_start;
    if (addr > ctx->address_mask || size - 1 > ctx->address_mask - addr) {
      continue;
    }
    *out = addr;
    return true;
  }
  return false;
}

enum dma_result dma_ctx_create(const struct dma_device_config* config,
                               struct dma_ctx** out) {
  if (config == NULL || out == NULL || config->address_mask == 0 ||
      (config->address_mask & (config->address_mask + 1)) != 0) {
    return DMA_ERROR_INVALID_INPUT;
  }
  *out = NULL;
  if (!ranges_valid(config->ranges, config->range_count)) {
    return DMA_ERROR_INVALID_INPUT;
  }
  if (!init_caches()) {
    return DMA_ERROR_INTERNAL;
  }

  struct dma_ctx* ctx = kmem_cache_zalloc(&ctx_cache);
  if (ctx == NULL) {
    return DMA_ERROR_NO_MEMORY;
  }
  size_t bytes = config->range_count * sizeof(struct dma_range);
  if (bytes > SIZE_MAX - (PMM_FRAME_SIZE - 1)) {
    kmem_cache_free(&ctx_cache, ctx);
    return DMA_ERROR_INVALID_INPUT;
  }
  size_t frames = (bytes + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
  uintptr_t phys;
  if (pmm_alloc(frames, &phys) != PMM_OK) {
    kmem_cache_free(&ctx_cache, ctx);
    return DMA_ERROR_NO_MEMORY;
  }
  ctx->ranges = (struct dma_range*)arch_phys_to_virt(phys);
  ctx->ranges_phys = phys;
  ctx->range_count = config->range_count;
  ctx->range_frames = frames;
  ctx->address_mask = config->address_mask;
  ctx->coherent = config->coherent;
  spinlock_init(&ctx->lock);
  for (size_t i = 0; i < ctx->range_count; ++i) {
    ctx->ranges[i] = config->ranges[i];
  }
  *out = ctx;
  return DMA_OK;
}

enum dma_result dma_ctx_destroy(struct dma_ctx* ctx) {
  if (ctx == NULL) {
    return DMA_ERROR_INVALID_INPUT;
  }
  bool irq = spin_lock_irqsave(&ctx->lock);
  bool busy = ctx->live_objects != 0;
  spin_unlock_irqrestore(&ctx->lock, irq);
  if (busy) {
    return DMA_ERROR_BUSY;
  }
  if (pmm_free(ctx->ranges_phys, ctx->range_frames) != PMM_OK ||
      kmem_cache_free(&ctx_cache, ctx) != 0) {
    return DMA_ERROR_INTERNAL;
  }
  return DMA_OK;
}

enum dma_result dma_alloc_coherent(struct dma_ctx* ctx, size_t size,
                                   size_t alignment, struct dma_buffer** out) {
  if (ctx == NULL || out == NULL || size == 0 || !is_power_of_two(alignment) ||
      size > SIZE_MAX - (PMM_FRAME_SIZE - 1)) {
    return DMA_ERROR_INVALID_INPUT;
  }
  *out = NULL;
  if (!ctx->coherent) {
    return DMA_ERROR_UNSUPPORTED;
  }
  if ((arch_phys_to_virt(0) & (alignment - 1)) != 0) {
    return DMA_ERROR_UNSUPPORTED;
  }
  size_t frames = (size + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
  size_t frame_bytes = frames * PMM_FRAME_SIZE;
  size_t phys_alignment =
      alignment > PMM_FRAME_SIZE ? alignment : PMM_FRAME_SIZE;
  struct dma_buffer* buffer = kmem_cache_zalloc(&buffer_cache);
  if (buffer == NULL) {
    return DMA_ERROR_NO_MEMORY;
  }

  bool candidate_window = false;
  for (size_t i = 0; i < ctx->range_count; ++i) {
    const struct dma_range* range = &ctx->ranges[i];
    if (range->device_start > ctx->address_mask ||
        range->device_start % alignment != range->phys_start % alignment) {
      continue;
    }
    size_t usable = range->size;
    dma_addr_t remaining = ctx->address_mask - range->device_start;
    if (usable - 1 > remaining) {
      usable = (size_t)(remaining + 1);
    }
    if (frame_bytes > usable) {
      continue;
    }
    candidate_window = true;

    uintptr_t phys;
    if (pmm_alloc_in_range(frames, range->phys_start,
                           range->phys_start + usable, phys_alignment,
                           &phys) != PMM_OK) {
      continue;
    }
    dma_addr_t addr;
    if (!direct_address(ctx, phys, frame_bytes, &addr)) {
      pmm_free(phys, frames);
      continue;
    }
    buffer->ctx = ctx;
    buffer->cpu_addr = (void*)arch_phys_to_virt(phys);
    buffer->device_addr = addr;
    buffer->phys = phys;
    buffer->size = size;
    buffer->frames = frames;
    ctx_add_object(ctx);
    *out = buffer;
    return DMA_OK;
  }
  kmem_cache_free(&buffer_cache, buffer);
  return candidate_window ? DMA_ERROR_NO_MEMORY : DMA_ERROR_NOT_ADDRESSABLE;
}

enum dma_result dma_free_coherent(struct dma_buffer* buffer) {
  if (buffer == NULL) {
    return DMA_ERROR_INVALID_INPUT;
  }
  if (pmm_free(buffer->phys, buffer->frames) != PMM_OK) {
    return DMA_ERROR_INTERNAL;
  }
  struct dma_ctx* ctx = buffer->ctx;
  if (kmem_cache_free(&buffer_cache, buffer) != 0) {
    return DMA_ERROR_INTERNAL;
  }
  ctx_remove_object(ctx);
  return DMA_OK;
}

void* dma_buffer_cpu_addr(const struct dma_buffer* buffer) {
  return buffer != NULL ? buffer->cpu_addr : NULL;
}

dma_addr_t dma_buffer_device_addr(const struct dma_buffer* buffer) {
  return buffer != NULL ? buffer->device_addr : 0;
}

size_t dma_buffer_size(const struct dma_buffer* buffer) {
  return buffer != NULL ? buffer->size : 0;
}

uintptr_t dma_buffer_phys_addr(const struct dma_buffer* buffer) {
  return buffer != NULL ? buffer->phys : 0;
}

size_t dma_buffer_allocation_size(const struct dma_buffer* buffer) {
  return buffer != NULL ? buffer->frames * PMM_FRAME_SIZE : 0;
}

static bool memory_contiguous(const struct dma_memory* memory,
                              uintptr_t* out_phys) {
  uintptr_t virt = (uintptr_t)memory->cpu_addr;
  if (memory->size > UINTPTR_MAX - virt) {
    return false;
  }
  struct vmm_space* space = vmm_get_kernel_space();
  uintptr_t phys;
  if (vmm_translate(space, virt, &phys) != VMM_OK ||
      memory->size - 1 > UINTPTR_MAX - phys) {
    return false;
  }
  size_t first_chunk = PMM_FRAME_SIZE - (virt % PMM_FRAME_SIZE);
  size_t done = first_chunk < memory->size ? first_chunk : memory->size;
  while (done < memory->size) {
    uintptr_t next_phys;
    if (vmm_translate(space, virt + done, &next_phys) != VMM_OK ||
        next_phys != phys + done) {
      return false;
    }
    size_t chunk = memory->size - done;
    done += chunk < PMM_FRAME_SIZE ? chunk : PMM_FRAME_SIZE;
  }
  *out_phys = phys;
  return true;
}

enum dma_result dma_map(struct dma_ctx* ctx, const struct dma_memory* memory,
                        enum dma_direction direction,
                        struct dma_mapping** out) {
  if (ctx == NULL || memory == NULL || memory->cpu_addr == NULL ||
      memory->size == 0 || out == NULL ||
      (direction != DMA_TO_DEVICE && direction != DMA_FROM_DEVICE &&
       direction != DMA_BIDIRECTIONAL)) {
    return DMA_ERROR_INVALID_INPUT;
  }
  *out = NULL;
  if (!ctx->coherent) {
    return DMA_ERROR_UNSUPPORTED;
  }
  uintptr_t phys;
  if (!memory_contiguous(memory, &phys)) {
    return DMA_ERROR_NOT_ADDRESSABLE;
  }
  dma_addr_t addr;
  if (!direct_address(ctx, phys, memory->size, &addr)) {
    return DMA_ERROR_NOT_ADDRESSABLE;
  }
  struct dma_mapping* mapping = kmem_cache_zalloc(&mapping_cache);
  if (mapping == NULL) {
    return DMA_ERROR_NO_MEMORY;
  }
  mapping->ctx = ctx;
  mapping->device_addr = addr;
  mapping->size = memory->size;
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
  ctx_add_object(ctx);
  *out = mapping;
  return DMA_OK;
}

enum dma_result dma_unmap(struct dma_mapping* mapping) {
  if (mapping == NULL) {
    return DMA_ERROR_INVALID_INPUT;
  }
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
  struct dma_ctx* ctx = mapping->ctx;
  if (kmem_cache_free(&mapping_cache, mapping) != 0) {
    return DMA_ERROR_INTERNAL;
  }
  ctx_remove_object(ctx);
  return DMA_OK;
}

dma_addr_t dma_mapping_device_addr(const struct dma_mapping* mapping) {
  return mapping != NULL ? mapping->device_addr : 0;
}

size_t dma_mapping_size(const struct dma_mapping* mapping) {
  return mapping != NULL ? mapping->size : 0;
}

enum dma_result dma_sync_for_device(struct dma_mapping* mapping) {
  if (mapping == NULL) {
    return DMA_ERROR_INVALID_INPUT;
  }
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
  return DMA_OK;
}

enum dma_result dma_sync_for_cpu(struct dma_mapping* mapping) {
  if (mapping == NULL) {
    return DMA_ERROR_INVALID_INPUT;
  }
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
  return DMA_OK;
}
