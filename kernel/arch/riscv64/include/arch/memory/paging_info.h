#pragma once

#define PAGE_SHIFT 12
#define PAGE_SIZE (1ULL << PAGE_SHIFT)  // 4096
#define PAGE_MASK (PAGE_SIZE - 1)
