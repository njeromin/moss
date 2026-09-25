#pragma once

#include <stddef.h>
#include <stdint.h>
#include <arch/memory/paging.h>

enum vmm_status {
  VMM_OK = 0,
  VMM_ERROR = -1,
};

struct vmm_space {
  phys_addr_t root_phys;
  size_t tag;
};

enum vmm_status vmm_init(void);

struct vmm_space* vmm_get_kernel_space(void);

// Create a new address space (allocates root page table and copies shared kernel mappings)
enum vmm_status vmm_create_space(struct vmm_space* space);

// Caller must first switch every hart away and stop all users of this space.
// Destroy a user address space and free its non-kernel page tables and tag.
enum vmm_status vmm_destroy_space(struct vmm_space* space);

// Switch the active hardware MMU address space
void vmm_switch_space(const struct vmm_space* space);

enum vmm_status vmm_map(struct vmm_space* space, virt_addr_t virt,
                        phys_addr_t phys, size_t size, uint8_t flags);

enum vmm_status vmm_unmap(struct vmm_space* space, virt_addr_t virt,
                          size_t size);

enum vmm_status vmm_unmap_page(struct vmm_space* space, virt_addr_t virt,
                               struct page_status* out_status);

enum vmm_status vmm_translate(const struct vmm_space* space, virt_addr_t virt,
                              phys_addr_t* phys);
