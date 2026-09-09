#pragma once

#include <stdbool.h>

// Returns true if hardware PTE Accessed/Dirty bit updating (Svadu) is enabled.
bool arch_has_svadu(void);
