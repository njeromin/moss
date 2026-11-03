#pragma once

#include <stddef.h>
#include <stdint.h>

// An address programmed into a device.
typedef uint64_t dma_addr_t;

struct dma_ctx;
struct dma_buffer;
struct dma_mapping;
struct dma_device_config;

// The caller owns and pins this kernel-mapped RAM until dma_unmap().
// A direct DMA context requires physically contiguous memory; an IOMMU context
// may map discontiguous pages.
struct dma_memory {
  void* cpu_addr;
  size_t size;
};

enum dma_direction {
  DMA_TO_DEVICE,
  DMA_FROM_DEVICE,
  DMA_BIDIRECTIONAL,
};

enum dma_result {
  DMA_OK = 0,
  DMA_ERROR_INVALID_INPUT = -1,
  DMA_ERROR_NO_MEMORY = -2,
  DMA_ERROR_NOT_ADDRESSABLE = -3,
  DMA_ERROR_UNSUPPORTED = -4,
  DMA_ERROR_BUSY = -5,
  DMA_ERROR_INTERNAL = -6,
};

// Trusted platform setup provides the direct-DMA configuration in
// <dma/config.h>. IOMMU-backed device assignment is not implemented yet.
// The context must outlive all its buffers and mappings; destroy returns
// DMA_ERROR_BUSY while any remain. Stop new uses before destroying a context.
enum dma_result dma_ctx_create(const struct dma_device_config* config,
                               struct dma_ctx** out);
enum dma_result dma_ctx_destroy(struct dma_ctx* ctx);

// Allocates memory visible to both CPU and device without explicit cache sync.
// Fails with DMA_ERROR_UNSUPPORTED if this guarantee cannot be provided.
// size must be nonzero; alignment must be a nonzero power of two and applies
// to both returned addresses.
// The device must stop using the buffer before it is freed.
enum dma_result dma_alloc_coherent(struct dma_ctx* ctx, size_t size,
                                   size_t alignment, struct dma_buffer** out);
enum dma_result dma_free_coherent(struct dma_buffer* buffer);
void* dma_buffer_cpu_addr(const struct dma_buffer* buffer);
dma_addr_t dma_buffer_device_addr(const struct dma_buffer* buffer);
size_t dma_buffer_size(const struct dma_buffer* buffer);
uintptr_t dma_buffer_phys_addr(const struct dma_buffer* buffer);
size_t dma_buffer_allocation_size(const struct dma_buffer* buffer);

// Maps existing memory for one device. The caller retains ownership and must
// not free or move it until unmap. The direct backend currently requires a
// cache-coherent device and physically contiguous memory in one DMA window.
// The device must finish DMA before unmap.
enum dma_result dma_map(struct dma_ctx* ctx, const struct dma_memory* memory,
                        enum dma_direction direction, struct dma_mapping** out);
enum dma_result dma_unmap(struct dma_mapping* mapping);
dma_addr_t dma_mapping_device_addr(const struct dma_mapping* mapping);
size_t dma_mapping_size(const struct dma_mapping* mapping);

// Transfer access to the device or CPU, including any required cache work.
// Call sync_for_device after CPU writes and before device access; call
// sync_for_cpu after device writes and before CPU access. Repeat when reusing
// a mapping. These calls do not replace ordering barriers for descriptors or
// device registers.
enum dma_result dma_sync_for_device(struct dma_mapping* mapping);
enum dma_result dma_sync_for_cpu(struct dma_mapping* mapping);
