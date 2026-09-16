#pragma once

#include <stdbool.h>

// Returns true if hardware PTE Accessed/Dirty bit updating (Svadu) is enabled.
bool arch_has_svadu(void);

// Returns true if forward-edge Control Flow Integrity landing pads (Zicfilp) are enabled.
bool arch_has_landing_pad(void);
