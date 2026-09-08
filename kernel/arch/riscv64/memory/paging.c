#include <arch/memory/layout.h>
#include <arch/memory/paging.h>
#include <arch/memory/paging_info.h>
#include <common/memory.h>
#include <core/memory/pmm.h>

#define SV39_ROOT_ENTRIES 512
#define SV39_USER_ENTRIES 256
#define SV39_KERNEL_ENTRIES 256
#define HHDM_ROOT_SLOT 256

#define SATP_MODE_SV39 (8ULL << 60)
#define SATP_ASID_SHIFT 44
#define SATP_ASID_MASK 0xFFFFULL
#define PTE_PPN_SHIFT 10

// Hardware PTE flags (Sv39)
#define PTE_VALID (1ULL << 0)
#define PTE_READ (1ULL << 1)
#define PTE_WRITE (1ULL << 2)
#define PTE_EXECUTE (1ULL << 3)
#define PTE_USER (1ULL << 4)
#define PTE_GLOBAL (1ULL << 5)
#define PTE_ACCESSED (1ULL << 6)
#define PTE_DIRTY (1ULL << 7)

// Extract 9-bit VPN index for level (2, 1, or 0)
static inline size_t vpn(virt_addr_t va, int level) {
  return (va >> (12 + (level * 9))) & 0x1FF;
}

// Convert PTE to its target physical address
static inline phys_addr_t pte_to_phys(uint64_t pte) {
  return ((pte >> PTE_PPN_SHIFT) << PAGE_SHIFT);
}

// Convert physical address to PTE PPN bits
static inline uint64_t phys_to_ppn(phys_addr_t pa) {
  return ((pa >> PAGE_SHIFT) << PTE_PPN_SHIFT);
}

static uint64_t* walk(phys_addr_t root, virt_addr_t virt, int alloc) {
  uint64_t* table = (uint64_t*)arch_phys_to_virt(root);

  // Traverse levels 2 and 1 down to level 0
  for (int level = 2; level > 0; level--) {
    size_t idx = vpn(virt, level);
    uint64_t pte = table[idx];

    if (pte & PTE_VALID) {
      if (pte & (PTE_READ | PTE_WRITE | PTE_EXECUTE)) {
        // Leaf huge page already occupies this slot; cannot traverse further
        return NULL;
      }
      table = (uint64_t*)arch_phys_to_virt(pte_to_phys(pte));
    } else {
      if (!alloc) {
        return NULL;
      }

      // Allocate an intermediate page table frame
      uintptr_t new_table_phys;
      if (pmm_alloc(1, &new_table_phys) != PMM_OK) {
        return NULL;
      }

      memory_set((void*)arch_phys_to_virt(new_table_phys), 0, PAGE_SIZE);

      // Point current PTE to the new table (Valid, no R/W/X makes it a branch)
      table[idx] = phys_to_ppn(new_table_phys) | PTE_VALID;
      table = (uint64_t*)arch_phys_to_virt(new_table_phys);
    }
  }

  // Return a pointer to the Level 0 PTE
  return &table[vpn(virt, 0)];
}

// Master kernel root saved during boot
static phys_addr_t master_kernel_root = 0;

int arch_paging_create_space(phys_addr_t* out_root) {
  if (out_root == NULL) {
    return PAGING_ERR_INVALID_ARG;
  }

  // Allocate a single 4 KiB physical frame for the root table
  uintptr_t phys;
  if (pmm_alloc(1, &phys) != PMM_OK) {
    return PAGING_ERR_NOMEM;
  }

  uint64_t* root_table = (uint64_t*)arch_phys_to_virt(phys);

  if (master_kernel_root == 0) {
    // Zero out all 512 entries
    memory_set(root_table, 0, PAGE_SIZE);

    // Direct-map physical RAM into the Higher-Half Direct Map (HHDM)
    // Root slots 256..383 cover 128 GiB of physical space.
    // DRAM starts at 0x80000000 (gigapage 2 -> slot 258).
    uint64_t hhdm_flags =
        PTE_VALID | PTE_READ | PTE_WRITE | PTE_ACCESSED | PTE_DIRTY;
    for (size_t g = 2; g < 128; g++) {
      size_t slot = HHDM_ROOT_SLOT + g;
      root_table[slot] = phys_to_ppn((phys_addr_t)g << 30) | hhdm_flags;
    }

    // Save as the template for future user spaces
    master_kernel_root = phys;
  } else {
    // Clear user entries (slots 0..255)
    memory_set(&root_table[0], 0, SV39_USER_ENTRIES * sizeof(uint64_t));

    // Share kernel mappings by copying the top-level kernel entries (slots 256..511)
    uint64_t* kernel_table = (uint64_t*)arch_phys_to_virt(master_kernel_root);
    memory_copy(&root_table[SV39_USER_ENTRIES],
                &kernel_table[SV39_USER_ENTRIES],
                SV39_KERNEL_ENTRIES * sizeof(uint64_t));
  }

  *out_root = phys;
  return PAGING_OK;
}

void arch_paging_destroy_space(phys_addr_t root) {
  if (root == 0 || root == master_kernel_root) {
    return;
  }

  uint64_t* table2 = (uint64_t*)arch_phys_to_virt(root);

  // Only free user mappings (slots 0..255). Never touch shared kernel slots (256..511).
  for (size_t idx2 = 0; idx2 < SV39_USER_ENTRIES; idx2++) {
    uint64_t pte2 = table2[idx2];
    if (!(pte2 & PTE_VALID) || (pte2 & (PTE_READ | PTE_WRITE | PTE_EXECUTE))) {
      continue;
    }

    phys_addr_t table1_phys = pte_to_phys(pte2);
    uint64_t* table1 = (uint64_t*)arch_phys_to_virt(table1_phys);

    for (size_t idx1 = 0; idx1 < SV39_ROOT_ENTRIES; idx1++) {
      uint64_t pte1 = table1[idx1];
      if (!(pte1 & PTE_VALID) ||
          (pte1 & (PTE_READ | PTE_WRITE | PTE_EXECUTE))) {
        continue;
      }

      phys_addr_t table0_phys = pte_to_phys(pte1);
      pmm_free(table0_phys, 1);
    }

    pmm_free(table1_phys, 1);
  }

  pmm_free(root, 1);
  arch_paging_flush_all();
}

void arch_paging_switch_space(phys_addr_t root, size_t space_id) {
  uint64_t satp_val =
      SATP_MODE_SV39 |
      (((uint64_t)space_id & SATP_ASID_MASK) << SATP_ASID_SHIFT) |
      (root >> PAGE_SHIFT);

  __asm__ volatile(
      "sfence.vma\n\t"     // Ensure all prior page-table stores are committed
      "csrw satp, %0\n\t"  // Switch translation mode, ASID, and root PPN
      "sfence.vma\n\t"
      :
      : "r"(satp_val)
      : "memory");
}

int arch_paging_map(phys_addr_t root, virt_addr_t virt, phys_addr_t phys,
                    unsigned permissions) {
  if ((virt & PAGE_MASK) != 0 || (phys & PAGE_MASK) != 0) {
    return PAGING_ERR_INVALID_ALIGNMENT;
  }

  uint64_t* pte = walk(root, virt, 1);
  if (pte == NULL) {
    return PAGING_ERR_NOMEM;
  }

  if (*pte & PTE_VALID) {
    return PAGING_ERR_ALREADY_MAPPED;
  }

  // Leaf flags: Valid, Accessed, Dirty (avoids early hardware page faults)
  uint64_t flags = PTE_VALID | PTE_ACCESSED | PTE_DIRTY;
  if (permissions & PAGE_READ) {
    flags |= PTE_READ;
  }
  if (permissions & PAGE_WRITE) {
    flags |= PTE_WRITE;
  }
  if (permissions & PAGE_EXEC) {
    flags |= PTE_EXECUTE;
  }
  if (permissions & PAGE_USER) {
    flags |= PTE_USER;
  }

  *pte = phys_to_ppn(phys) | flags;
  arch_paging_flush_page(virt);
  return PAGING_OK;
}

static inline int is_table_empty(const uint64_t* table) {
  for (size_t i = 0; i < SV39_ROOT_ENTRIES; i++) {
    if (table[i] != 0) {
      return 0;
    }
  }
  return 1;
}

int arch_paging_unmap(phys_addr_t root, virt_addr_t virt) {
  if ((virt & PAGE_MASK) != 0) {
    return PAGING_ERR_INVALID_ALIGNMENT;
  }

  uint64_t* table2 = (uint64_t*)arch_phys_to_virt(root);
  size_t idx2 = vpn(virt, 2);
  uint64_t pte2 = table2[idx2];
  if (!(pte2 & PTE_VALID) || (pte2 & (PTE_READ | PTE_WRITE | PTE_EXECUTE))) {
    return PAGING_ERR_NOT_MAPPED;
  }

  phys_addr_t table1_phys = pte_to_phys(pte2);
  uint64_t* table1 = (uint64_t*)arch_phys_to_virt(table1_phys);
  size_t idx1 = vpn(virt, 1);
  uint64_t pte1 = table1[idx1];
  if (!(pte1 & PTE_VALID) || (pte1 & (PTE_READ | PTE_WRITE | PTE_EXECUTE))) {
    return PAGING_ERR_NOT_MAPPED;
  }

  phys_addr_t table0_phys = pte_to_phys(pte1);
  uint64_t* table0 = (uint64_t*)arch_phys_to_virt(table0_phys);
  size_t idx0 = vpn(virt, 0);
  uint64_t pte0 = table0[idx0];
  if (!(pte0 & PTE_VALID)) {
    return PAGING_ERR_NOT_MAPPED;
  }

  // 1. Clear the leaf 4 KiB entry and invalidate the TLB for this virtual address
  table0[idx0] = 0;
  arch_paging_flush_page(virt);

  // 2. Prune Level 0 table if it has no remaining entries
  if (is_table_empty(table0)) {
    table1[idx1] = 0;
    pmm_free(table0_phys, 1);

    // 3. Prune Level 1 table if empty (never prune shared kernel entries in user spaces)
    int is_shared_kernel_slot =
        (root != master_kernel_root) && (idx2 >= SV39_USER_ENTRIES);
    if (!is_shared_kernel_slot && is_table_empty(table1)) {
      table2[idx2] = 0;
      pmm_free(table1_phys, 1);
    }
  }

  return PAGING_OK;
}

int arch_paging_query(phys_addr_t root, virt_addr_t virt, phys_addr_t* out_phys,
                      unsigned* out_permissions) {
  uint64_t* table = (uint64_t*)arch_phys_to_virt(root);

  for (int level = 2; level >= 0; level--) {
    size_t idx = vpn(virt, level);
    uint64_t pte = table[idx];

    if (!(pte & PTE_VALID)) {
      return PAGING_ERR_NOT_MAPPED;
    }

    // Leaf PTE has at least one of R, W, X set (or it's level 0)
    if ((pte & (PTE_READ | PTE_WRITE | PTE_EXECUTE)) || level == 0) {
      if (out_phys) {
        size_t offset_mask = (1ULL << (12 + (level * 9))) - 1;
        *out_phys = (pte_to_phys(pte) & ~offset_mask) | (virt & offset_mask);
      }
      if (out_permissions) {
        unsigned perms = 0;
        if (pte & PTE_READ) {
          perms |= PAGE_READ;
        }
        if (pte & PTE_WRITE) {
          perms |= PAGE_WRITE;
        }
        if (pte & PTE_EXECUTE) {
          perms |= PAGE_EXEC;
        }
        if (pte & PTE_USER) {
          perms |= PAGE_USER;
        }
        *out_permissions = perms;
      }
      return PAGING_OK;
    }

    table = (uint64_t*)arch_phys_to_virt(pte_to_phys(pte));
  }

  return PAGING_ERR_NOT_MAPPED;
}

void arch_paging_flush_page(virt_addr_t virt) {
  asm volatile("sfence.vma %0, zero" : : "r"(virt) : "memory");
}

void arch_paging_flush_all(void) {
  asm volatile("sfence.vma zero, zero" ::: "memory");
}
