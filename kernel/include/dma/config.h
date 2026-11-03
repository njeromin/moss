#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <dma/direct.h>

// Filled by trusted platform setup, not by a user driver.
struct dma_device_config {
  // A nonzero mask of contiguous low device-address bits.
  dma_addr_t address_mask;
  // True when the device participates in CPU cache coherence.
  bool coherent;
  // Physical-to-device address windows used when no IOMMU is attached.
  const struct dma_range* ranges;
  size_t range_count;
};
