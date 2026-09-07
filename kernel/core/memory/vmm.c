#include <core/memory/vmm.h>

#include <core/memory/pmm.h>

int vmm_map(struct vmm_space* space, uintptr_t virt, uintptr_t phys,
            size_t size, uint8_t flags) {}

int vmm_unmap(struct vmm_space* space, uintptr_t virt, size_t size) {}

int vmm_translate(const struct vmm_space* space, uintptr_t virt,
                  uintptr_t* phys) {}
