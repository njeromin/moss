/* Run from the repository root:
 cc -std=c11 -Wall -Wextra -Werror -Ikernel/include -Icommon/include \
 -Itests/dma/include tests/dma/direct_test.c kernel/dma/dma.c \
 kernel/memory/pmm.c kernel/memory/kmem_cache.c \
 common/memory.c -o /tmp/dma_direct_test && /tmp/dma_direct_test
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <memory/boot_alloc.h>
#include <memory/memory_map.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <dma/config.h>
#include <dma/dma.h>

static _Alignas(8192) unsigned char ram[4096 * 64];
static _Alignas(4096) unsigned char metadata[4096];
static size_t metadata_used;
static struct memory_region available = {.offset = (uintptr_t)ram,
                                         .size = sizeof(ram)};
static struct memory_map memory_map = {.available = &available,
                                       .available_count = 1,
                                       .available_capacity = 1};
static struct vmm_space kernel_space = {.root_phys = 1};
static uintptr_t gap_page;
static bool interrupts_enabled = true;

const struct memory_map* memory_map_get(void) {
  return &memory_map;
}

void* boot_alloc(size_t size, size_t alignment) {
  size_t start = (metadata_used + alignment - 1) & ~(alignment - 1);
  if (start > sizeof(metadata) || size > sizeof(metadata) - start) {
    return NULL;
  }
  metadata_used = start + size;
  return metadata + start;
}

void boot_alloc_finish(void) {}

uintptr_t arch_phys_to_virt(uintptr_t phys) {
  return phys;
}

uintptr_t arch_virt_to_phys(uintptr_t virt) {
  return virt;
}

bool arch_interrupts_enabled(void) {
  return interrupts_enabled;
}

void arch_interrupts_disable(void) {
  interrupts_enabled = false;
}

void arch_interrupts_enable(void) {
  interrupts_enabled = true;
}

struct vmm_space* vmm_get_kernel_space(void) {
  return &kernel_space;
}

enum vmm_status vmm_translate(const struct vmm_space* space, uintptr_t virt,
                              uintptr_t* phys) {
  if (space != &kernel_space || phys == NULL || virt < (uintptr_t)ram ||
      virt >= (uintptr_t)ram + sizeof(ram)) {
    return VMM_ERROR;
  }
  *phys = virt + (gap_page != 0 && virt >= gap_page ? 4096 : 0);
  return VMM_OK;
}

int main(void) {
  assert(pmm_init() == PMM_OK);
  struct dma_range range = {.phys_start = (uintptr_t)ram + 16 * 4096,
                            .device_start = 0x100000,
                            .size = 32 * 4096};
  struct dma_device_config config = {.address_mask = 0x1fffff,
                                     .coherent = true,
                                     .ranges = &range,
                                     .range_count = 1};
  struct dma_ctx* ctx;
  assert(dma_ctx_create(&config, &ctx) == DMA_OK);
  range.device_start = 0;  // The context owns its copy.

  struct dma_buffer* buffer;
  assert(dma_alloc_coherent(ctx, 6000, 8192, &buffer) == DMA_OK);
  assert(((uintptr_t)dma_buffer_cpu_addr(buffer) & 8191) == 0);
  assert((dma_buffer_device_addr(buffer) & 8191) == 0);
  assert(dma_buffer_device_addr(buffer) ==
         0x100000 + (uintptr_t)dma_buffer_cpu_addr(buffer) - range.phys_start);
  assert(dma_buffer_size(buffer) == 6000);
  assert(dma_ctx_destroy(ctx) == DMA_ERROR_BUSY);
  assert(dma_free_coherent(buffer) == DMA_OK);

  uintptr_t phys;
  assert(pmm_alloc_in_range(2, range.phys_start, range.phys_start + range.size,
                            4096, &phys) == PMM_OK);
  struct dma_memory memory = {.cpu_addr = (void*)phys, .size = 8192};
  struct dma_mapping* mapping;
  assert(dma_map(ctx, &memory, DMA_BIDIRECTIONAL, &mapping) == DMA_OK);
  assert(dma_mapping_device_addr(mapping) ==
         0x100000 + phys - range.phys_start);
  assert(dma_mapping_size(mapping) == 8192);
  assert(dma_sync_for_device(mapping) == DMA_OK);
  assert(dma_sync_for_cpu(mapping) == DMA_OK);
  assert(dma_ctx_destroy(ctx) == DMA_ERROR_BUSY);
  assert(dma_unmap(mapping) == DMA_OK);

  gap_page = phys + 4096;
  assert(dma_map(ctx, &memory, DMA_TO_DEVICE, &mapping) ==
         DMA_ERROR_NOT_ADDRESSABLE);
  gap_page = 0;
  assert(pmm_free(phys, 2) == PMM_OK);

  uintptr_t outside;
  assert(pmm_alloc_in_range(1, (uintptr_t)ram, range.phys_start, 4096,
                            &outside) == PMM_OK);
  memory = (struct dma_memory){.cpu_addr = (void*)outside, .size = 4096};
  assert(dma_map(ctx, &memory, DMA_TO_DEVICE, &mapping) ==
         DMA_ERROR_NOT_ADDRESSABLE);
  assert(pmm_free(outside, 1) == PMM_OK);
  assert(dma_ctx_destroy(ctx) == DMA_OK);

  range.device_start = 0x100100;
  config.address_mask = 0x1fffff;
  assert(dma_ctx_create(&config, &ctx) == DMA_OK);
  assert(dma_alloc_coherent(ctx, 4096, 64, &buffer) == DMA_OK);
  assert((dma_buffer_device_addr(buffer) & 63) == 0);
  assert(dma_free_coherent(buffer) == DMA_OK);
  assert(dma_ctx_destroy(ctx) == DMA_OK);

  range.device_start = 0x100000;
  config.address_mask = 0xffff;
  assert(dma_ctx_create(&config, &ctx) == DMA_OK);
  assert(dma_alloc_coherent(ctx, 4096, 4096, &buffer) ==
         DMA_ERROR_NOT_ADDRESSABLE);
  assert(dma_ctx_destroy(ctx) == DMA_OK);

  config.address_mask = 0x1fffff;
  config.coherent = false;
  assert(dma_ctx_create(&config, &ctx) == DMA_OK);
  assert(dma_alloc_coherent(ctx, 4096, 4096, &buffer) == DMA_ERROR_UNSUPPORTED);
  assert(dma_ctx_destroy(ctx) == DMA_OK);
  return 0;
}
