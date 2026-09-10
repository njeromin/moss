#include <core/memory/vmm.h>

#include <arch/memory/layout.h>
#include <asm/paging_info.h>
#include <common/memory.h>
#include <core/memory/direct_map.h>
#include <core/memory/memory_map.h>
#include <core/memory/pmm.h>

static struct vmm_space kernel_space;

enum vmm_status vmm_init(void) {
  if (arch_paging_create_space(&kernel_space.root_phys) != 0) {
    return VMM_ERROR;
  }

  if (direct_map_init(&kernel_space, memory_map_get()) != VMM_OK) {
    return VMM_ERROR;
  }

  // Map kernel ELF segments individually with W^X protections:
  //   .text:             PAGE_READ | PAGE_EXEC
  //   .rodata:           PAGE_READ
  //   .data:             PAGE_READ | PAGE_WRITE
  //   .bss and stack:    PAGE_READ | PAGE_WRITE
  uintptr_t text_vstart = align_down((uintptr_t)text_start, PAGE_SIZE);
  uintptr_t text_vend = align_up((uintptr_t)text_end, PAGE_SIZE);
  size_t text_size = text_vend - text_vstart;
  if (text_size > 0) {
    int err = vmm_map(&kernel_space, text_vstart,
                      arch_kernel_virt_to_phys(text_vstart), text_size,
                      PAGE_READ | PAGE_EXEC);
    if (err != VMM_OK) {
      return err;
    }
  }

  uintptr_t rodata_vstart = align_down((uintptr_t)rodata_start, PAGE_SIZE);
  uintptr_t rodata_vend = align_up((uintptr_t)rodata_end, PAGE_SIZE);
  size_t rodata_size = rodata_vend - rodata_vstart;
  if (rodata_size > 0) {
    int err = vmm_map(&kernel_space, rodata_vstart,
                      arch_kernel_virt_to_phys(rodata_vstart), rodata_size,
                      PAGE_READ);
    if (err != VMM_OK) {
      return err;
    }
  }

  uintptr_t data_vstart = align_down((uintptr_t)data_start, PAGE_SIZE);
  uintptr_t data_vend = align_up((uintptr_t)data_end, PAGE_SIZE);
  size_t data_size = data_vend - data_vstart;
  if (data_size > 0) {
    int err = vmm_map(&kernel_space, data_vstart,
                      arch_kernel_virt_to_phys(data_vstart), data_size,
                      PAGE_READ | PAGE_WRITE);
    if (err != VMM_OK) {
      return err;
    }
  }

  uintptr_t bss_vstart = align_down((uintptr_t)bss_start, PAGE_SIZE);
  uintptr_t bss_vend = align_up((uintptr_t)kernel_end, PAGE_SIZE);
  size_t bss_size = bss_vend - bss_vstart;
  if (bss_size > 0) {
    int err =
        vmm_map(&kernel_space, bss_vstart, arch_kernel_virt_to_phys(bss_vstart),
                bss_size, PAGE_READ | PAGE_WRITE);
    if (err != VMM_OK) {
      return err;
    }
  }

  arch_paging_switch_space(kernel_space.root_phys, 0);

  return VMM_OK;
}

struct vmm_space* vmm_get_kernel_space(void) {
  return &kernel_space;
}

enum vmm_status vmm_map(struct vmm_space* space, uintptr_t virt, uintptr_t phys,
                        size_t size, uint8_t flags) {
  if (space == NULL || (virt & PAGE_MASK) != 0 || (phys & PAGE_MASK) != 0) {
    return VMM_ERROR;
  }

  return arch_paging_map_range(space->root_phys, virt, phys, size, flags) ==
                 PAGING_OK
             ? VMM_OK
             : VMM_ERROR;
}

enum vmm_status vmm_unmap(struct vmm_space* space, uintptr_t virt,
                          size_t size) {
  if (space == NULL || size == 0) {
    return VMM_ERROR;
  }

  // Both virt and size must be page-aligned
  if ((virt & PAGE_MASK) != 0 || (size & PAGE_MASK) != 0) {
    return VMM_ERROR;
  }

  // Overflow check (virt + size wrapping around 64-bit integer)
  if (virt + size < virt) {
    return VMM_ERROR;
  }

  return arch_paging_unmap_range(space->root_phys, virt, size) == PAGING_OK
             ? VMM_OK
             : VMM_ERROR;
}

enum vmm_status vmm_unmap_page(struct vmm_space* space, virt_addr_t virt,
                               struct page_status* out_status) {
  if (space == NULL || (virt & PAGE_MASK) != 0) {
    return VMM_ERROR;
  }

  int res = arch_paging_unmap_get_status(space->root_phys, virt, out_status);
  if (res != PAGING_OK) {
    return VMM_ERROR;
  }

  return VMM_OK;
}

enum vmm_status vmm_translate(const struct vmm_space* space, uintptr_t virt,
                              uintptr_t* phys) {
  if (space == NULL || phys == NULL || space->root_phys == 0) {
    return VMM_ERROR;
  }

  // Query the hardware page table; we pass NULL for permissions since we only want phys
  int res = arch_paging_query(space->root_phys, virt, phys, NULL);
  if (res != PAGING_OK) {
    return VMM_ERROR;
  }

  return VMM_OK;
}
