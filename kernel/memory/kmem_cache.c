#include <memory/kmem_cache.h>

#include <arch/memory/layout.h>
#include <arch/memory/paging.h>
#include <common/memory.h>
#include <memory/pmm.h>
#include <sync/spinlock.h>

#define ITEM_ALIGNMENT 8

int kmem_cache_init(struct kmem_cache* cache, const char* name,
                    size_t item_size) {
  if (cache == NULL) {
    return -1;
  }

  if (item_size < sizeof(void*)) {
    item_size = sizeof(void*);
  }

  size_t aligned_size = align_up(item_size, ITEM_ALIGNMENT);
  size_t header_size = align_up(sizeof(struct kmem_slab), ITEM_ALIGNMENT);

  if (header_size + aligned_size > PMM_FRAME_SIZE) {
    return -1;
  }

  cache->name = name;
  cache->item_size = aligned_size;
  cache->slabs = NULL;
  spinlock_init(&cache->lock);

  return 0;
}

static struct kmem_slab* alloc_new_slab(struct kmem_cache* cache) {
  uintptr_t phys_addr;
  if (pmm_alloc(1, &phys_addr) != PMM_OK) {
    return NULL;
  }

  uintptr_t virt_addr = arch_phys_to_virt(phys_addr);

  struct kmem_slab* slab = (struct kmem_slab*)virt_addr;
  slab->in_use = 0;
  slab->next = cache->slabs;
  cache->slabs = slab;

  uintptr_t start_of_items =
      align_up(virt_addr + sizeof(struct kmem_slab), ITEM_ALIGNMENT);
  slab->free_list = (void*)start_of_items;

  uintptr_t current = start_of_items;
  while (current + cache->item_size <= virt_addr + PMM_FRAME_SIZE) {
    uintptr_t next = current + cache->item_size;

    if (next + cache->item_size <= virt_addr + PMM_FRAME_SIZE) {
      *(void**)current = (void*)next;
    } else {
      *(void**)current = NULL;
    }

    current = next;
  }

  return slab;
}

void* kmem_cache_alloc(struct kmem_cache* cache) {
  if (cache == NULL) {
    return NULL;
  }

  bool irq = spin_lock_irqsave(&cache->lock);

  struct kmem_slab* slab = cache->slabs;
  while (slab != NULL && slab->free_list == NULL) {
    slab = slab->next;
  }

  if (slab == NULL) {
    slab = alloc_new_slab(cache);
    if (slab == NULL) {
      spin_unlock_irqrestore(&cache->lock, irq);
      return NULL;
    }
  }

  void* allocated_item = slab->free_list;
  slab->free_list = *(void**)allocated_item;
  slab->in_use++;

  spin_unlock_irqrestore(&cache->lock, irq);

  return allocated_item;
}

void* kmem_cache_zalloc(struct kmem_cache* cache) {
  void* item = kmem_cache_alloc(cache);
  if (item != NULL) {
    memory_set(item, 0, cache->item_size);
  }
  return item;
}

int kmem_cache_free(struct kmem_cache* cache, void* item) {
  if (cache == NULL || item == NULL) {
    return -1;
  }

  bool irq = spin_lock_irqsave(&cache->lock);

  struct kmem_slab* slab = cache->slabs;
  uintptr_t item_addr = (uintptr_t)item;

  while (slab != NULL) {
    uintptr_t page_start = (uintptr_t)slab;
    uintptr_t page_end = page_start + PMM_FRAME_SIZE;

    if (item_addr >= page_start && item_addr < page_end) {
      uintptr_t items_start =
          align_up(page_start + sizeof(struct kmem_slab), ITEM_ALIGNMENT);

      if (item_addr < items_start ||
          (item_addr - items_start) % cache->item_size != 0) {
        spin_unlock_irqrestore(&cache->lock, irq);
        return -2;
      }

      *(void**)item = slab->free_list;
      slab->free_list = item;
      slab->in_use--;

      spin_unlock_irqrestore(&cache->lock, irq);
      return 0;
    }

    slab = slab->next;
  }

  spin_unlock_irqrestore(&cache->lock, irq);
  return -3;
}

int kmem_cache_destroy(struct kmem_cache* cache) {
  if (cache == NULL) {
    return -1;
  }

  bool irq = spin_lock_irqsave(&cache->lock);

  struct kmem_slab* slab = cache->slabs;
  while (slab != NULL) {
    if (slab->in_use > 0) {
      spin_unlock_irqrestore(&cache->lock, irq);
      return -2;
    }
    slab = slab->next;
  }

  slab = cache->slabs;
  while (slab != NULL) {
    struct kmem_slab* next = slab->next;
    uintptr_t phys_addr = arch_virt_to_phys((uintptr_t)slab);
    pmm_free(phys_addr, 1);
    slab = next;
  }

  cache->slabs = NULL;

  spin_unlock_irqrestore(&cache->lock, irq);

  return 0;
}
