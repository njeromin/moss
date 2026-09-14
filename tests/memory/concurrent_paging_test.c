/* Host stress test using the real PMM and page-table code, with CPU/TLB stubs:
 cc -std=gnu11 -O2 -Wall -Wextra -Werror -pthread -Ikernel/include \
 -Icommon/include -Ikernel/arch/riscv64/include \
 tests/memory/concurrent_paging_test.c kernel/memory/pmm.c \
 kernel/arch/riscv64/memory/paging/paging.c common/memory.c \
 -o /tmp/concurrent_paging_test && /tmp/concurrent_paging_test
 */
#include <arch/cpu.h>
#include <arch/memory/paging.h>
#include <assert.h>
#include <memory/boot_alloc.h>
#include <memory/pmm.h>
#include <pthread.h>
#include <sched.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define THREADS 8
#define ROUNDS 1000
static _Alignas(4096) unsigned char ram[16 * 1024 * 1024];
static unsigned owners[sizeof(ram) / 4096];
static struct memory_region available, reserved;
static struct memory_map map;
static _Thread_local bool interrupts = true;
static uintptr_t root, other;
static pthread_barrier_t barrier;
static unsigned mapped_count, unmapped_count;

bool arch_interrupts_enabled(void) {
  return interrupts;
}

void arch_interrupts_disable(void) {
  interrupts = false;
}

void arch_interrupts_enable(void) {
  interrupts = true;
}

bool arch_has_svadu(void) {
  return true;
}

uintptr_t arch_phys_to_virt(uintptr_t phys) {
  return phys;
}

void riscv64_paging_activate(phys_addr_t phys, size_t id) {
  (void)phys;
  (void)id;
  assert(!interrupts);
}

void arch_paging_flush_all(void) {
  assert(!interrupts);
  sched_yield();
}

void arch_paging_flush_page(uintptr_t virt) {
  (void)virt;
  arch_paging_flush_all();
}

void kpanic(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  abort();
}

const struct memory_map* boot_alloc_memory_map(void) {
  return &map;
}

const struct memory_map* memory_map_get(void) {
  return &map;
}

void boot_alloc_finish(void) {}

void* boot_alloc(size_t size, size_t alignment) {
  uintptr_t next =
      (reserved.offset + reserved.size + alignment - 1) & ~(alignment - 1);
  reserved.size = next + size - reserved.offset;
  assert(reserved.size < sizeof(ram));
  return (void*)next;
}

static void* worker(void* argument) {
  size_t id = (size_t)argument;
  uintptr_t va = 0x40000000ULL + id * 4096;
  for (size_t i = 0; i < ROUNDS; ++i) {
    uintptr_t frame, translated;
    bool irq = (i & 1) != 0;
    interrupts = irq;
    assert(pmm_alloc(1, &frame) == PMM_OK);
    assert(interrupts == irq);
    size_t index = (frame - (uintptr_t)ram) / 4096;
    assert(__atomic_exchange_n(&owners[index], 1, __ATOMIC_SEQ_CST) == 0);
    assert(arch_paging_map(root, va, frame, PAGE_READ | PAGE_WRITE) ==
           PAGING_OK);
    assert(interrupts == irq);
    assert(arch_paging_query(root, va, &translated, NULL) == PAGING_OK);
    assert(translated == frame);
    assert(arch_paging_unmap(root, va) == PAGING_OK);
    assert(interrupts == irq);
    assert(__atomic_exchange_n(&owners[index], 0, __ATOMIC_SEQ_CST) == 1);
    assert(pmm_free(frame, 1) == PMM_OK);
    assert(interrupts == irq);
  }
  interrupts = true;
  // Exactly one competing mapper/unmapper can succeed for the same address.
  pthread_barrier_wait(&barrier);
  int result = arch_paging_map(root, 0x80000000ULL, 0, PAGE_READ);
  assert(result == PAGING_OK || result == PAGING_ERR_ALREADY_MAPPED);
  if (result == PAGING_OK) {
    __atomic_fetch_add(&mapped_count, 1, __ATOMIC_RELAXED);
  }
  pthread_barrier_wait(&barrier);
  result = arch_paging_unmap(root, 0x80000000ULL);
  assert(result == PAGING_OK || result == PAGING_ERR_NOT_MAPPED);
  if (result == PAGING_OK) {
    __atomic_fetch_add(&unmapped_count, 1, __ATOMIC_RELAXED);
  }
  pthread_barrier_wait(&barrier);
  // Concurrent partial unmaps split the same large leaf without losing siblings.
  assert(arch_paging_unmap(root, 4ULL * 1024 * 1024 * 1024 + id * 4096) ==
         PAGING_OK);
  // Shared kernel tables must be protected even with different root handles.
  uintptr_t shared_va = HHDM_BASE + id * 4096;
  uintptr_t mine = (id & 1) ? root : other;
  uintptr_t peer = (id & 1) ? other : root;
  for (size_t i = 0; i < 100; ++i) {
    uintptr_t translated;
    assert(arch_paging_map(mine, shared_va, 0, PAGE_READ) == PAGING_OK);
    assert(arch_paging_query(peer, shared_va, &translated, NULL) == PAGING_OK);
    assert(translated == 0);
    assert(arch_paging_unmap(peer, shared_va) == PAGING_OK);
  }
  return NULL;
}

static void* query_worker(void* ignored) {
  (void)ignored;
  for (size_t i = 0; i < ROUNDS * THREADS; ++i) {
    uintptr_t phys;
    int result = arch_paging_query(root, 0x40000000ULL + (i % THREADS) * 4096,
                                   &phys, NULL);
    assert(result == PAGING_OK || result == PAGING_ERR_NOT_MAPPED);
    struct page_status status;
    result = arch_paging_get_status(root, 0x40000000ULL, &status);
    assert(result == PAGING_OK || result == PAGING_ERR_NOT_MAPPED);
  }
  return NULL;
}

int main(void) {
  available =
      (struct memory_region){.offset = (uintptr_t)ram, .size = sizeof(ram)};
  reserved = (struct memory_region){.offset = (uintptr_t)ram};
  map = (struct memory_map){.available = &available,
                            .available_count = 1,
                            .reserved = &reserved,
                            .reserved_count = 1};
  assert(pmm_init() == PMM_OK);
  assert(arch_paging_create_space(&root) == PAGING_OK);
  assert(arch_paging_create_space(&other) == PAGING_OK);
  assert(arch_paging_map_range(root, 4ULL << 30, 0, 1ULL << 30,
                               PAGE_READ | PAGE_WRITE) == PAGING_OK);
  pthread_t threads[THREADS], reader;
  assert(pthread_barrier_init(&barrier, NULL, THREADS) == 0);
  for (size_t i = 0; i < THREADS; ++i) {
    assert(!pthread_create(&threads[i], NULL, worker, (void*)i));
  }
  assert(!pthread_create(&reader, NULL, query_worker, NULL));
  for (size_t i = 0; i < THREADS; ++i) {
    assert(!pthread_join(threads[i], NULL));
  }
  assert(!pthread_join(reader, NULL));
  assert(mapped_count == 1 && unmapped_count == 1);
  uintptr_t phys;
  for (size_t i = 0; i < THREADS; ++i) {
    assert(arch_paging_query(root, (4ULL << 30) + i * 4096, &phys, NULL) ==
           PAGING_ERR_NOT_MAPPED);
  }
  assert(arch_paging_query(root, (4ULL << 30) + THREADS * 4096, &phys, NULL) ==
         PAGING_OK);
  assert(phys == THREADS * 4096);
  arch_paging_destroy_space(other);
  puts("Concurrent PMM and paging tests passed (8 writers, 1 reader).");
}
