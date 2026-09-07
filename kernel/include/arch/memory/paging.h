#pragma once

#include <stdint.h>

typedef uintptr_t phys_addr_t;
typedef uintptr_t virt_addr_t;

enum page_permissions {
  PAGE_READ = 1u << 0,
  PAGE_WRITE = 1u << 1,
  PAGE_EXEC = 1u << 2,
  PAGE_USER = 1u << 3,
};

/* Allocate a zeroed, kernel-accessible page-table frame.
     Both addresses must refer to the same frame. */
struct page_table_allocator {
  int (*alloc)(phys_addr_t* phys, void** writable);
  void (*free)(phys_addr_t phys);
  void* (*access)(phys_addr_t phys);
};

int arch_paging_init(const struct page_table_allocator* allocator);

/* Map/unmap one base page in the kernel address space. */
int arch_paging_map(virt_addr_t virt, phys_addr_t phys, unsigned permissions);
int arch_paging_unmap(virt_addr_t virt);

/* Accepts an address with a page offset; returns its translation. */
int arch_paging_query(virt_addr_t virt, phys_addr_t* phys,
                      unsigned* permissions);

/* Activate the kernel tables built by this backend. */
void arch_paging_activate(void);
