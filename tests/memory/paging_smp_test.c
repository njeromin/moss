// Test-only HSM startup; the production kernel still boots a single hart.
#include "sbi.h"
#include <arch/memory/layout.h>
#include <arch/memory/paging.h>
#include <arch/trap.h>
#include <core/console.h>
#include <core/memory/pmm.h>
#include <core/panic.h>

#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c))                                                                  \
      kpanic("SMP paging test failed: %s", #c);                                \
  } while (0)
#define VA 0x40000000ULL
static _Alignas(16) unsigned char stack[16384];

static struct {
  uint64_t satp, sp, entry;
} startup;

static uintptr_t test_root;
static unsigned stage;
extern const uintptr_t paging_smp_entry_address;

static void wait_stage(unsigned wanted) {
  while (__atomic_load_n(&stage, __ATOMIC_ACQUIRE) != wanted) {
    __asm__ volatile("nop");
  }
}

static void mapping_worker(uintptr_t va) {
  for (size_t i = 0; i < 256; ++i) {
    uintptr_t frame, translated;
    CHECK(pmm_alloc(1, &frame) == PMM_OK);
    CHECK(arch_paging_map(test_root, va, frame, PAGE_READ | PAGE_WRITE) ==
          PAGING_OK);
    CHECK(arch_paging_query(test_root, va, &translated, NULL) == PAGING_OK);
    CHECK(translated == frame);
    CHECK(arch_paging_unmap(test_root, va) == PAGING_OK);
    CHECK(pmm_free(frame, 1) == PMM_OK);
  }
}

static void secondary(void) {
  arch_trap_init();
  CHECK(*(volatile uint64_t*)VA == 11);
  __atomic_store_n(&stage, 1, __ATOMIC_RELEASE);
  wait_stage(2);
  // This hart cached VA -> first frame. The main hart replaced it remotely.
  CHECK(*(volatile uint64_t*)VA == 22);
  mapping_worker(VA + 0x200000);
  __atomic_store_n(&stage, 3, __ATOMIC_RELEASE);
  sbi_hart_stop();
  kpanic("Secondary hart failed to stop");
}

void paging_smp_test(void) {
  unsigned long hart;
  // QEMU's test configurations use contiguous small hart IDs. Select a stopped
  // hart instead of assuming which hart OpenSBI chose to boot the kernel.
  for (hart = 0; hart < 64; ++hart) {
    struct sbi_ret result = sbi_hart_get_status(hart);
    if (result.error == SBI_SUCCESS && result.value == 1) {
      break;
    }
  }
  if (hart == 64) {
    console_log("paging_test",
                "SMP test skipped: no stopped secondary hart.\n");
    return;
  }
  uintptr_t first, second;
  CHECK(pmm_alloc(1, &first) == PMM_OK);
  CHECK(pmm_alloc(1, &second) == PMM_OK);
  *(uint64_t*)arch_phys_to_virt(first) = 11;
  *(uint64_t*)arch_phys_to_virt(second) = 22;
  CHECK(arch_paging_create_space(&test_root) == PAGING_OK);
  CHECK(arch_paging_map(test_root, VA, first, PAGE_READ | PAGE_WRITE) ==
        PAGING_OK);
  uintptr_t entry = paging_smp_entry_address;  // linker places .init physically
  CHECK(arch_paging_map(test_root, entry & ~4095ULL, entry & ~4095ULL,
                        PAGE_READ | PAGE_EXEC) == PAGING_OK);
  startup.satp = (8ULL << 60) | (test_root >> 12);
  startup.sp = (uintptr_t)(stack + sizeof(stack));
  startup.entry = (uintptr_t)secondary;
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
  CHECK(
      sbi_hart_start(hart, entry, arch_kernel_virt_to_phys((uintptr_t)&startup))
          .error == SBI_SUCCESS);
  wait_stage(1);
  CHECK(arch_paging_unmap(test_root, VA) == PAGING_OK);
  CHECK(arch_paging_map(test_root, VA, second, PAGE_READ | PAGE_WRITE) ==
        PAGING_OK);
  __atomic_store_n(&stage, 2, __ATOMIC_RELEASE);
  mapping_worker(VA + 0x201000);
  wait_stage(3);
  while (sbi_hart_get_status(hart).value != 1) {
    __asm__ volatile("nop");
  }
  arch_paging_destroy_space(test_root);
  CHECK(pmm_free(first, 1) == PMM_OK);
  CHECK(pmm_free(second, 1) == PMM_OK);
  console_log("paging_test",
              "Concurrent hart mapping and remote TLB shootdown passed.\n");
}
