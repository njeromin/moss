#pragma once

#include <arch/memory/paging.h>
#include <stddef.h>
#include <stdint.h>

enum vmm_status {
  VMM_OK = 0,
  VMM_ERROR = -1,
};

struct vmm_space {
  phys_addr_t root_phys;
};

enum vmm_status vmm_init(void);

struct vmm_space* vmm_get_kernel_space(void);

enum vmm_status vmm_map(struct vmm_space* space, virt_addr_t virt,
                        phys_addr_t phys, size_t size, uint8_t flags);

enum vmm_status vmm_unmap(struct vmm_space* space, virt_addr_t virt,
                          size_t size);

enum vmm_status vmm_translate(const struct vmm_space* space, virt_addr_t virt,
                              phys_addr_t* phys);
