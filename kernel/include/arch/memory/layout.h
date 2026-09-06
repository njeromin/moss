#pragma once

#include <stdint.h>

// Translate an address in the fixed higher-half kernel mapping. The selected
// architecture supplies the implementation; this does not walk page tables.
uintptr_t arch_kernel_virt_to_phys(uintptr_t virt);
