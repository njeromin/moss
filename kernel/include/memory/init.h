#pragma once

#include <core/kmain.h>

// Boot time initialisation of memory discovery, allocators, and kernel mappings.
void init_virtual_memory(struct device_discovery_info device_discovery);
