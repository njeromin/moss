#include <arch/memory/layout.h>

#include <common/fdt.h>

// These values are emitted by the architecture linker script.
extern const uintptr_t kernel_virtual_base;
extern const uintptr_t kernel_physical_base;

uintptr_t arch_kernel_virt_to_phys(uintptr_t virt) {
  return virt - kernel_virtual_base + kernel_physical_base;
}

int arch_get_memory_map(
    const struct device_discovery_info device_discovery_info,
    struct memory_region* regions, size_t capacity, size_t* count) {
  // TODO: Implement
  return 0;
}
