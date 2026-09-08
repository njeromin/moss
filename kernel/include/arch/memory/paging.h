#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uintptr_t phys_addr_t;
typedef uintptr_t virt_addr_t;

/* Access permissions (static policy / protection) */
enum page_permissions {
  PAGE_READ = 1u << 0,
  PAGE_WRITE = 1u << 1,
  PAGE_EXEC = 1u << 2,
  PAGE_USER = 1u << 3,
};

/* Dynamic hardware page status flags */
enum page_status_flags {
  PAGE_STATUS_ACCESSED = 1u << 0,
  PAGE_STATUS_DIRTY = 1u << 1,
};

/* Dynamic hardware page status */
struct page_status {
  bool accessed;
  bool dirty;
};

enum paging_result {
  PAGING_OK = 0,
  PAGING_ERR_NOMEM = -1,
  PAGING_ERR_ALREADY_MAPPED = -2,
  PAGING_ERR_NOT_MAPPED = -3,
  PAGING_ERR_INVALID_ALIGNMENT = -4,
  PAGING_ERR_INVALID_ARG = -5,
};

// Create a new address space root table with shared kernel mappings copied in.
int arch_paging_create_space(phys_addr_t* out_root);

// Free an address space and its non-kernel intermediate page tables.
void arch_paging_destroy_space(phys_addr_t root);

// Switch the hardware MMU to the given root (e.g. writes satp on RISC-V).
void arch_paging_switch_space(phys_addr_t root, size_t space_id);

/* Map one base 4KiB page in the specified root page table. */
int arch_paging_map(phys_addr_t root, virt_addr_t virt, phys_addr_t phys,
                    unsigned permissions);

// Unmap one base 4KiB page and flush TLB.
int arch_paging_unmap(phys_addr_t root, virt_addr_t virt);

// Query translation and permissions for a virtual address.
int arch_paging_query(phys_addr_t root, virt_addr_t virt, phys_addr_t* out_phys,
                      unsigned* out_permissions);

// Query dynamic hardware status (accessed, dirty) for a mapped virtual address.
int arch_paging_get_status(phys_addr_t root, virt_addr_t virt,
                           struct page_status* out_status);

// Clear dynamic hardware status flags (e.g. PAGE_STATUS_ACCESSED, PAGE_STATUS_DIRTY) and flush TLB.
int arch_paging_clear_status(phys_addr_t root, virt_addr_t virt,
                             unsigned status_flags);

// Set dynamic hardware status flags and flush TLB.
int arch_paging_set_status(phys_addr_t root, virt_addr_t virt,
                           unsigned status_flags);

// Invalidate the TLB entry for a single page (used after unmap or permission change)
void arch_paging_flush_page(virt_addr_t virt);

// Invalidate all TLB entries (used when switching address spaces or tearing down a space)
void arch_paging_flush_all(void);
