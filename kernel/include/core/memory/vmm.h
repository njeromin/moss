#pragma once

#include <stddef.h>
#include <stdint.h>

struct vmm_space {
  uintptr_t root_phys;
};

int vmm_map(struct vmm_space* space, uintptr_t virt, uintptr_t phys,
            size_t size, uint8_t flags);

int vmm_unmap(struct vmm_space* space, uintptr_t virt, size_t size);

int vmm_translate(const struct vmm_space* space, uintptr_t virt,
                  uintptr_t* phys);
