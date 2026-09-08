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

uintptr_t arch_phys_to_virt(uintptr_t phys) {
  return phys + HHDM_BASE;
}

uintptr_t arch_virt_to_phys(uintptr_t virt) {
  return virt - HHDM_BASE;
}
