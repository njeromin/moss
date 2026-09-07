#pragma once

#include <arch/memory/paging.h>
#include <stddef.h>
#include <stdint.h>

struct vmm_space {
  phys_addr_t root_phys;
};

int vmm_init(void);

int vmm_map(struct vmm_space* space, virt_addr_t virt, phys_addr_t phys,
            size_t size, uint8_t flags);

int vmm_unmap(struct vmm_space* space, virt_addr_t virt, size_t size);

int vmm_translate(const struct vmm_space* space, virt_addr_t virt,
                  phys_addr_t* phys);
