#include "./paging_hw.h"
#include "sbi.h"
#include <asm/paging_info.h>
#include <core/panic.h>

void riscv64_paging_activate(phys_addr_t root, size_t space_id) {
  uint64_t satp =
      (8ULL << 60) | ((space_id & 0xffffULL) << 44) | (root >> PAGE_SHIFT);
  __asm__ volatile("sfence.vma\n\tcsrw satp, %0\n\tsfence.vma"
                   :
                   : "r"(satp)
                   : "memory");
}

void arch_paging_flush_all(void) {
  // Publish stores before asking other harts to invalidate their translations.
  // SBI RFENCE runs in machine mode, so remote harts can service it while
  // spinning on paging_lock with supervisor interrupts disabled.
  __asm__ volatile("fence rw, rw\n\tsfence.vma zero, zero" ::: "memory");
  // hart_mask_base=-1 selects all available harts; size=-1 flushes all VAs.
  struct sbi_ret result = sbi_remote_sfence_vma(0, ~0UL, 0, ~0UL);
  if (result.error != SBI_SUCCESS) {
    // Never return page-table frames to the PMM after an incomplete shootdown.
    kpanic("SBI TLB shootdown failed (%ld)", result.error);
  }
}

void arch_paging_flush_page(virt_addr_t virt) {
  (void)virt;
  // Use the conservative all-address/all-ASID operation for shared mappings
  // and large leaves as well as base pages.
  arch_paging_flush_all();
}
