#pragma once

// Kernel fixed-size object cache allocator (kmem_cache).

#include <stddef.h>
#include <stdint.h>
#include <sync/spinlock.h>

struct kmem_slab {
  void* free_list;
  size_t in_use;
  struct kmem_slab* next;
};

struct kmem_cache {
  const char* name;
  size_t item_size;
  struct kmem_slab* slabs;
  struct spinlock lock;
};

int kmem_cache_init(struct kmem_cache* cache, const char* name, size_t item_size);
void* kmem_cache_alloc(struct kmem_cache* cache);
void* kmem_cache_zalloc(struct kmem_cache* cache);
int kmem_cache_free(struct kmem_cache* cache, void* item);
int kmem_cache_destroy(struct kmem_cache* cache);
