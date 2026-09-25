#include <arch/memory/paging.h>

#include <arch/cpu.h>
#include <arch/memory/layout.h>
#include <asm/hw_features.h>
#include <asm/paging_info.h>
#include <common/memory.h>
#include <core/panic.h>
#include <memory/pmm.h>
#include <sync/spinlock.h>
#include "./paging_hw.h"

// Shared kernel tables require one lock across all roots. Lock order is
// paging_lock -> PMM lock. Queries hold this lock too, protecting table lifetime.
static struct spinlock paging_lock = SPINLOCK_INIT;

// ASIDs are local to each hart, but one system-wide allocation lets a space
// retain its tag when scheduled on another hart. Zero is never allocated.
static uint8_t allocated_asids[65536 / 8];
static size_t asid_max;
static bool asids_initialised;

void arch_paging_init_cpu(void) {
  uint64_t original;
  uint64_t observed;
  __asm__ volatile("csrr %0, satp" : "=r"(original));
  uint64_t probe = original | (0xffffULL << 44);
  __asm__ volatile(
      "csrw satp, %1\n\tcsrr %0, satp\n\tcsrw satp, %2\n\tsfence.vma zero, zero"
      : "=&r"(observed)
      : "r"(probe), "r"(original)
      : "memory");
  size_t supported = (observed >> 44) & 0xffffULL;
  if (!asids_initialised) {
    asid_max = supported;
    asids_initialised = true;
  } else if (supported < asid_max) {
    kpanic("Secondary CPU has too few ASID bits.\n");
  }
}

size_t arch_paging_alloc_tag(void) {
  bool irq = spin_lock_irqsave(&paging_lock);
  for (size_t tag = 1; tag <= asid_max; ++tag) {
    size_t byte = tag / 8;
    uint8_t bit = 1u << (tag % 8);
    if (!(allocated_asids[byte] & bit)) {
      allocated_asids[byte] |= bit;
      spin_unlock_irqrestore(&paging_lock, irq);
      return tag;
    }
  }
  spin_unlock_irqrestore(&paging_lock, irq);
  return 0;
}

void arch_paging_release_tag(size_t tag) {
  if (tag == 0) {
    return;
  }
  bool irq = spin_lock_irqsave(&paging_lock);
  if (tag > asid_max || !(allocated_asids[tag / 8] & (1u << (tag % 8)))) {
    kpanic("Releasing an unallocated ASID.\n");
  }
  allocated_asids[tag / 8] &= ~(1u << (tag % 8));
  spin_unlock_irqrestore(&paging_lock, irq);
}

#define SV39_ROOT_ENTRIES 512
#define SV39_USER_ENTRIES 256
#define SV39_KERNEL_ENTRIES 256

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

static size_t leaf_size(int level) {
  return 1ULL << (PAGE_SHIFT + 9 * level);
}

// Find the largest empty leaf slot the request can use. Existing branches
// force smaller leaves; existing mappings must never be overwritten.
static int walk_map(phys_addr_t root, virt_addr_t virt, phys_addr_t phys,
                    size_t remaining, uint64_t** out_pte, size_t* out_size) {
  uint64_t* table = (uint64_t*)arch_phys_to_virt(root);
  for (int level = 2; level >= 0; --level) {
    uint64_t* pte = &table[vpn(virt, level)];
    uint64_t value = __atomic_load_n(pte, __ATOMIC_ACQUIRE);
    size_t size = leaf_size(level);
    if (value & PTE_VALID) {
      if ((value & (PTE_READ | PTE_WRITE | PTE_EXECUTE)) || level == 0) {
        return PAGING_ERR_ALREADY_MAPPED;
      }
    } else {
      if (remaining >= size && ((virt | phys) & (size - 1)) == 0) {
        *out_pte = pte;
        *out_size = size;
        return PAGING_OK;
      }
      uintptr_t child;
      if (pmm_alloc(1, &child) != PMM_OK) {
        return PAGING_ERR_NOMEM;
      }
      memory_set((void*)arch_phys_to_virt(child), 0, PAGE_SIZE);
      value = phys_to_ppn(child) | PTE_VALID;
      __atomic_store_n(pte, value, __ATOMIC_RELEASE);
    }
    table = (uint64_t*)arch_phys_to_virt(pte_to_phys(value));
  }
  return PAGING_ERR_INVALID_ARG;
}

// Preserve translation, permissions and conservative A/D history while
// replacing a large leaf with 512 smaller leaves. Callers serialise software
// page-table changes with paging_lock; CAS also preserves hardware A/D updates.
static int split_leaf(uint64_t* pte, int level) {
  uintptr_t child;
  if (pmm_alloc(1, &child) != PMM_OK) {
    return PAGING_ERR_NOMEM;
  }
  uint64_t* table = (uint64_t*)arch_phys_to_virt(child);
  uint64_t old = __atomic_load_n(pte, __ATOMIC_ACQUIRE);
  uint64_t branch = phys_to_ppn(child) | PTE_VALID;
  do {
    uintptr_t base = pte_to_phys(old);
    uint64_t flags = old & ((1ULL << PTE_PPN_SHIFT) - 1);
    for (size_t i = 0; i < SV39_ROOT_ENTRIES; ++i) {
      table[i] = phys_to_ppn(base + i * leaf_size(level - 1)) | flags;
    }
  } while (!__atomic_compare_exchange_n(pte, &old, branch, false,
                                        __ATOMIC_SEQ_CST, __ATOMIC_ACQUIRE));
  // Flush the old large translation before removing any child. Keeping the
  // replacement valid also allows splitting the HHDM containing these tables.
  arch_paging_flush_all();
  return PAGING_OK;
}

// Traverse page table to locate the leaf PTE (level 2 huge, level 1 large, or level 0 base page)
static uint64_t* walk_leaf(phys_addr_t root, virt_addr_t virt) {
  uint64_t* table = (uint64_t*)arch_phys_to_virt(root);

  for (int level = 2; level >= 0; level--) {
    size_t idx = vpn(virt, level);
    uint64_t* pte = &table[idx];
    uint64_t pte_val = __atomic_load_n(pte, __ATOMIC_ACQUIRE);

    if (!(pte_val & PTE_VALID)) {
      return NULL;
    }

    if ((pte_val & (PTE_READ | PTE_WRITE | PTE_EXECUTE)) || level == 0) {
      return pte;
    }

    table = (uint64_t*)arch_phys_to_virt(pte_to_phys(pte_val));
  }

  return NULL;
}

// Master kernel root saved during boot
static phys_addr_t master_kernel_root = 0;

static int create_space_locked(phys_addr_t* out_root) {
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

    // Save as the template for future user spaces
    master_kernel_root = phys;
  } else {
    // Clear user entries (slots 0..255)
    memory_set(&root_table[0], 0, SV39_USER_ENTRIES * sizeof(uint64_t));

    // Share kernel mappings by copying the top-level kernel entries (slots 256..511)
    uint64_t* kernel_table = (uint64_t*)arch_phys_to_virt(master_kernel_root);
    // Root entries are copied, but lower tables are shared. Demote kernel
    // root leaves before copying so later partial unmaps update a shared table
    // rather than replacing a root entry in only one address space.
    for (size_t i = SV39_USER_ENTRIES; i < SV39_ROOT_ENTRIES; ++i) {
      uint64_t pte = kernel_table[i];
      if (!(pte & PTE_VALID)) {
        uintptr_t child;
        if (pmm_alloc(1, &child) != PMM_OK) {
          pmm_free(phys, 1);
          return PAGING_ERR_NOMEM;
        }
        memory_set((void*)arch_phys_to_virt(child), 0, PAGE_SIZE);
        __atomic_store_n(&kernel_table[i], phys_to_ppn(child) | PTE_VALID,
                         __ATOMIC_RELEASE);
      } else if (pte & (PTE_READ | PTE_WRITE | PTE_EXECUTE)) {
        int result = split_leaf(&kernel_table[i], 2);
        if (result != PAGING_OK) {
          pmm_free(phys, 1);
          return result;
        }
      }
    }
    memory_copy(&root_table[SV39_USER_ENTRIES],
                &kernel_table[SV39_USER_ENTRIES],
                SV39_KERNEL_ENTRIES * sizeof(uint64_t));
  }

  *out_root = phys;
  return PAGING_OK;
}

static void destroy_space_locked(phys_addr_t root) {
  if (root == 0 || root == master_kernel_root) {
    return;
  }

  arch_paging_flush_all();

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

static void switch_space_locked(phys_addr_t root, size_t tag) {
  riscv64_paging_activate(root, tag);
}

static int map_leaf(phys_addr_t root, virt_addr_t virt, phys_addr_t phys,
                    size_t remaining, unsigned permissions, size_t* mapped) {
  if ((virt & PAGE_MASK) != 0 || (phys & PAGE_MASK) != 0) {
    return PAGING_ERR_INVALID_ALIGNMENT;
  }

  if (!(permissions & (PAGE_READ | PAGE_EXEC)) ||
      ((permissions & PAGE_WRITE) && !(permissions & PAGE_READ))) {
    return PAGING_ERR_INVALID_ARG;
  }

  uint64_t* pte;
  int result = walk_map(root, virt, phys, remaining, &pte, mapped);
  if (result != PAGING_OK) {
    return result;
  }

  uint64_t flags = PTE_VALID;
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

  // Handle Svadu (hardware A/D update) vs non-Svadu (software / Svade)
  if (arch_has_svadu()) {
    if (!(permissions & PAGE_USER)) {
      // For kernel pages, pre-set Accessed (and Dirty if writable) to avoid
      // unnecessary atomic bus writes on permanent kernel mappings.
      flags |= PTE_ACCESSED;
      if (permissions & PAGE_WRITE) {
        flags |= PTE_DIRTY;
      }
    }
    // For user pages, leave A and D un-set (0) so hardware autonomously
    // tracks the first read/write.
  } else {
    // Non-Svadu: Hardware does not update A/D bits automatically. Pre-set
    // Accessed, and Dirty if writable, preventing traps on systems without
    // software A/D fault handlers.
    flags |= PTE_ACCESSED;
    if (permissions & PAGE_WRITE) {
      flags |= PTE_DIRTY;
    }
  }

  __atomic_store_n(pte, phys_to_ppn(phys) | flags, __ATOMIC_RELEASE);
  return PAGING_OK;
}

static int map_locked(phys_addr_t root, virt_addr_t virt, phys_addr_t phys,
                      unsigned permissions) {
  if (root == 0) {
    return PAGING_ERR_INVALID_ARG;
  }
  size_t mapped;
  int result = map_leaf(root, virt, phys, PAGE_SIZE, permissions, &mapped);
  arch_paging_flush_all();
  return result;
}

static int map_range_locked(phys_addr_t root, virt_addr_t virt,
                            phys_addr_t phys, size_t size,
                            unsigned permissions) {
  if (root == 0 || size == 0 || (virt & PAGE_MASK) || (phys & PAGE_MASK) ||
      (size & PAGE_MASK) || size > UINTPTR_MAX - virt ||
      size > UINTPTR_MAX - phys) {
    return PAGING_ERR_INVALID_ARG;
  }

  int result = PAGING_OK;
  for (size_t offset = 0; offset < size;) {
    size_t mapped;
    result = map_leaf(root, virt + offset, phys + offset, size - offset,
                      permissions, &mapped);
    if (result != PAGING_OK) {
      break;
    }
    offset += mapped;
  }
  // Publish the whole batch, including new intermediate tables. Flush even
  // on failure because earlier pages (or intermediate tables) may be installed.
  arch_paging_flush_all();
  return result;
}

static inline int is_table_empty(const uint64_t* table) {
  for (size_t i = 0; i < SV39_ROOT_ENTRIES; i++) {
    if (__atomic_load_n(&table[i], __ATOMIC_RELAXED) != 0) {
      return 0;
    }
  }
  return 1;
}

static int unmap_ext_locked(phys_addr_t root, virt_addr_t virt,
                            struct page_status* out_status,
                            phys_addr_t* out_phys) {
  if (root == 0) {
    return PAGING_ERR_INVALID_ARG;
  }
  if ((virt & PAGE_MASK) != 0) {
    return PAGING_ERR_INVALID_ALIGNMENT;
  }

  uint64_t* table2 = (uint64_t*)arch_phys_to_virt(root);
  size_t idx2 = vpn(virt, 2);
  uint64_t pte2 = __atomic_load_n(&table2[idx2], __ATOMIC_ACQUIRE);
  if (!(pte2 & PTE_VALID)) {
    return PAGING_ERR_NOT_MAPPED;
  }
  if (pte2 & (PTE_READ | PTE_WRITE | PTE_EXECUTE)) {
    int result = split_leaf(&table2[idx2], 2);
    if (result != PAGING_OK) {
      return result;
    }
    pte2 = __atomic_load_n(&table2[idx2], __ATOMIC_ACQUIRE);
  }

  phys_addr_t table1_phys = pte_to_phys(pte2);
  uint64_t* table1 = (uint64_t*)arch_phys_to_virt(table1_phys);
  size_t idx1 = vpn(virt, 1);
  uint64_t pte1 = __atomic_load_n(&table1[idx1], __ATOMIC_ACQUIRE);
  if (!(pte1 & PTE_VALID)) {
    return PAGING_ERR_NOT_MAPPED;
  }
  if (pte1 & (PTE_READ | PTE_WRITE | PTE_EXECUTE)) {
    int result = split_leaf(&table1[idx1], 1);
    if (result != PAGING_OK) {
      return result;
    }
    pte1 = __atomic_load_n(&table1[idx1], __ATOMIC_ACQUIRE);
  }

  phys_addr_t table0_phys = pte_to_phys(pte1);
  uint64_t* table0 = (uint64_t*)arch_phys_to_virt(table0_phys);
  size_t idx0 = vpn(virt, 0);

  // Atomically clear the leaf entry. This ensures race-free unmapping when
  // hardware Svadu A/D updates occur concurrently, while capturing the final
  // dynamic Accessed and Dirty state.
  uint64_t old_pte = __atomic_exchange_n(&table0[idx0], 0, __ATOMIC_SEQ_CST);
  if (!(old_pte & PTE_VALID)) {
    return PAGING_ERR_NOT_MAPPED;
  }

  // Invalidate the TLB entry for this virtual address
  arch_paging_flush_page(virt);

  // Return dynamic Accessed/Dirty status if requested
  if (out_status != NULL) {
    out_status->accessed = (old_pte & PTE_ACCESSED) != 0;
    out_status->dirty = (old_pte & PTE_DIRTY) != 0;
  }

  // Return physical address if requested
  if (out_phys != NULL) {
    *out_phys = pte_to_phys(old_pte);
  }

  // Prune Level 0 table if it has no remaining entries
  if (is_table_empty(table0)) {
    __atomic_exchange_n(&table1[idx1], 0, __ATOMIC_SEQ_CST);

    // Kernel root entries may have been copied to other spaces. Keep their
    // shared Level 1 tables alive, including when operating on the master.
    int is_shared_kernel_slot = (idx2 >= SV39_USER_ENTRIES);
    bool prune_l1 = false;
    if (!is_shared_kernel_slot && is_table_empty(table1)) {
      __atomic_exchange_n(&table2[idx2], 0, __ATOMIC_SEQ_CST);
      prune_l1 = true;
    }

    // Invalidate non-leaf translation caches before returning table frames to PMM
    // to prevent hardware walkers from using stale page table hierarchy entries.
    arch_paging_flush_all();

    pmm_free(table0_phys, 1);
    if (prune_l1) {
      pmm_free(table1_phys, 1);
    }
  }

  return PAGING_OK;
}

int arch_paging_unmap_get_status(phys_addr_t root, virt_addr_t virt,
                                 struct page_status* out_status) {
  return arch_paging_unmap_ext(root, virt, out_status, NULL);
}

int arch_paging_unmap(phys_addr_t root, virt_addr_t virt) {
  return arch_paging_unmap_ext(root, virt, NULL, NULL);
}

static int query_locked(phys_addr_t root, virt_addr_t virt,
                        phys_addr_t* out_phys, unsigned* out_permissions) {
  uint64_t* table = (uint64_t*)arch_phys_to_virt(root);

  for (int level = 2; level >= 0; level--) {
    size_t idx = vpn(virt, level);
    uint64_t pte = __atomic_load_n(&table[idx], __ATOMIC_ACQUIRE);

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

static int get_status_locked(phys_addr_t root, virt_addr_t virt,
                             struct page_status* out_status) {
  if ((virt & PAGE_MASK) != 0) {
    return PAGING_ERR_INVALID_ALIGNMENT;
  }
  if (out_status == NULL) {
    return PAGING_ERR_INVALID_ARG;
  }

  uint64_t* pte = walk_leaf(root, virt);
  if (pte == NULL) {
    return PAGING_ERR_NOT_MAPPED;
  }

  uint64_t pte_val = __atomic_load_n(pte, __ATOMIC_ACQUIRE);
  if (!(pte_val & PTE_VALID)) {
    return PAGING_ERR_NOT_MAPPED;
  }

  out_status->accessed = (pte_val & PTE_ACCESSED) != 0;
  out_status->dirty = (pte_val & PTE_DIRTY) != 0;
  return PAGING_OK;
}

static int clear_status_locked(phys_addr_t root, virt_addr_t virt,
                               unsigned status_flags) {
  if ((virt & PAGE_MASK) != 0) {
    return PAGING_ERR_INVALID_ALIGNMENT;
  }

  uint64_t* pte = walk_leaf(root, virt);
  if (pte == NULL) {
    return PAGING_ERR_NOT_MAPPED;
  }

  uint64_t pte_mask = 0;
  if (status_flags & PAGE_STATUS_ACCESSED) {
    pte_mask |= PTE_ACCESSED;
  }
  if (status_flags & PAGE_STATUS_DIRTY) {
    pte_mask |= PTE_DIRTY;
  }

  // Atomically clear status bits without clobbering concurrent hardware Svadu updates
  uint64_t old = __atomic_fetch_and(pte, ~pte_mask, __ATOMIC_SEQ_CST);
  if (!(old & PTE_VALID)) {
    return PAGING_ERR_NOT_MAPPED;
  }

  arch_paging_flush_page(virt);
  return PAGING_OK;
}

static int set_status_locked(phys_addr_t root, virt_addr_t virt,
                             unsigned status_flags) {
  if ((virt & PAGE_MASK) != 0) {
    return PAGING_ERR_INVALID_ALIGNMENT;
  }

  uint64_t* pte = walk_leaf(root, virt);
  if (pte == NULL) {
    return PAGING_ERR_NOT_MAPPED;
  }

  uint64_t pte_bits = 0;
  if (status_flags & PAGE_STATUS_ACCESSED) {
    pte_bits |= PTE_ACCESSED;
  }
  if (status_flags & PAGE_STATUS_DIRTY) {
    pte_bits |= PTE_DIRTY;
  }

  // Atomically set status bits
  uint64_t old = __atomic_fetch_or(pte, pte_bits, __ATOMIC_SEQ_CST);
  if (!(old & PTE_VALID)) {
    return PAGING_ERR_NOT_MAPPED;
  }

  arch_paging_flush_page(virt);
  return PAGING_OK;
}

static int unmap_range_locked(phys_addr_t root, virt_addr_t virt, size_t size) {
  if (!root || !size || (virt & PAGE_MASK) || (size & PAGE_MASK) ||
      size > UINTPTR_MAX - virt) {
    return PAGING_ERR_INVALID_ARG;
  }
  for (size_t offset = 0; offset < size; offset += PAGE_SIZE) {
    int result = unmap_ext_locked(root, virt + offset, NULL, NULL);
    if (result != PAGING_OK) {
      return result;
    }
  }
  return PAGING_OK;
}

int arch_paging_create_space(phys_addr_t* out_root) {
  bool irq = spin_lock_irqsave(&paging_lock);
  int result = create_space_locked(out_root);
  spin_unlock_irqrestore(&paging_lock, irq);
  return result;
}

void arch_paging_destroy_space(phys_addr_t root) {
  bool irq = spin_lock_irqsave(&paging_lock);
  destroy_space_locked(root);
  spin_unlock_irqrestore(&paging_lock, irq);
}

int arch_paging_map(phys_addr_t root, virt_addr_t virt, phys_addr_t phys,
                    unsigned permissions) {
  bool irq = spin_lock_irqsave(&paging_lock);
  int result = map_locked(root, virt, phys, permissions);
  spin_unlock_irqrestore(&paging_lock, irq);
  return result;
}

int arch_paging_map_range(phys_addr_t root, virt_addr_t virt, phys_addr_t phys,
                          size_t size, unsigned permissions) {
  bool irq = spin_lock_irqsave(&paging_lock);
  int result = map_range_locked(root, virt, phys, size, permissions);
  spin_unlock_irqrestore(&paging_lock, irq);
  return result;
}

int arch_paging_unmap_ext(phys_addr_t root, virt_addr_t virt,
                          struct page_status* out_status,
                          phys_addr_t* out_phys) {
  bool irq = spin_lock_irqsave(&paging_lock);
  int result = unmap_ext_locked(root, virt, out_status, out_phys);
  spin_unlock_irqrestore(&paging_lock, irq);
  return result;
}

int arch_paging_query(phys_addr_t root, virt_addr_t virt, phys_addr_t* out_phys,
                      unsigned* out_permissions) {
  bool irq = spin_lock_irqsave(&paging_lock);
  int result = query_locked(root, virt, out_phys, out_permissions);
  spin_unlock_irqrestore(&paging_lock, irq);
  return result;
}

int arch_paging_get_status(phys_addr_t root, virt_addr_t virt,
                           struct page_status* out_status) {
  bool irq = spin_lock_irqsave(&paging_lock);
  int result = get_status_locked(root, virt, out_status);
  spin_unlock_irqrestore(&paging_lock, irq);
  return result;
}

int arch_paging_clear_status(phys_addr_t root, virt_addr_t virt,
                             unsigned status_flags) {
  bool irq = spin_lock_irqsave(&paging_lock);
  int result = clear_status_locked(root, virt, status_flags);
  spin_unlock_irqrestore(&paging_lock, irq);
  return result;
}

int arch_paging_set_status(phys_addr_t root, virt_addr_t virt,
                           unsigned status_flags) {
  bool irq = spin_lock_irqsave(&paging_lock);
  int result = set_status_locked(root, virt, status_flags);
  spin_unlock_irqrestore(&paging_lock, irq);
  return result;
}

int arch_paging_unmap_range(phys_addr_t root, virt_addr_t virt, size_t size) {
  bool irq = spin_lock_irqsave(&paging_lock);
  int result = unmap_range_locked(root, virt, size);
  spin_unlock_irqrestore(&paging_lock, irq);
  return result;
}

void arch_paging_switch_space(phys_addr_t root, size_t tag) {
  bool irq = spin_lock_irqsave(&paging_lock);
  switch_space_locked(root, tag);
  spin_unlock_irqrestore(&paging_lock, irq);
}
