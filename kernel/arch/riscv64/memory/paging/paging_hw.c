#include "./paging_hw.h"
#include <sbi.h>
#include <asm/paging_info.h>
#include <core/panic.h>

void riscv64_paging_activate(phys_addr_t root, size_t tag) {
  uint64_t satp =
      (8ULL << 60) | ((tag & 0xffffULL) << 44) | (root >> PAGE_SHIFT);
  uint64_t current_satp;
  __asm__ volatile("csrr %0, satp" : "=r"(current_satp));
  if (current_satp == satp) {
    return;
  }

  __asm__ volatile("csrw satp, %0" : : "r"(satp) : "memory");

  // Allocated ASIDs remain bound to their roots until a synchronous all-hart
  // flush at destruction. ASID zero can be shared, so fence every switch to it.
  if (tag == 0) {
    __asm__ volatile("sfence.vma zero, zero" ::: "memory");
  }
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
