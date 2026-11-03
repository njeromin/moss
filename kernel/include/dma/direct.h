#pragma once

#include <stddef.h>
#include <stdint.h>
#include <dma/dma.h>

// A direct DMA window. The two starts refer to the same byte of memory.
// Windows must not overlap in physical or device address space.
struct dma_range {
  uintptr_t phys_start;
  dma_addr_t device_start;
  size_t size;
};
