#include <arch/memory/layout.h>

// These values are emitted by the architecture linker script.
extern const uintptr_t kernel_virtual_base;
extern const uintptr_t kernel_physical_base;

uintptr_t arch_kernel_virt_to_phys(uintptr_t virt) {
  return virt - kernel_virtual_base + kernel_physical_base;
}

uintptr_t arch_kernel_phys_to_virt(uintptr_t phys) {
  return phys + kernel_virtual_base - kernel_physical_base;
}

struct memory_region arch_boot_alloc_window(void) {
  // Matches the bootstrap identity leaf in entry.S.
  return (struct memory_region){.offset = 0x80000000ULL, .size = 0x40000000ULL};
}
