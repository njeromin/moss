#pragma once

#define PAGE_SHIFT 12
#define PAGE_SIZE (1ULL << PAGE_SHIFT)  // 4096
#define PAGE_MASK (PAGE_SIZE - 1)

// Physical-address window reserved for the direct map.
#define DIRECT_MAP_PHYS_LIMIT (128ULL << 30)
