#include "arch/memory/layout.h"
#include "arch/panic.h"
#include "core/memory/regions.h"
#include "core/memory/vmm.h"
#include "core/panic.h"
#include <core/kmain.h>

#include <core/console.h>
#include <core/memory/allocation/boot_alloc.h>
#include <core/memory/pmm.h>

#define _CONSOLE_LOG_SUBJECT "kmain"
#define _CONSOLE_LOG_SUBJECT_MEM_INIT "kmain:memory_init"

void init_virtual_memory(const struct device_discovery_info device_discovery) {
  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT,
              "Initialising memory management.\n");

  struct memory_region available[MAX_MEMORY_REGIONS];
  struct memory_region reserved[MAX_MEMORY_REGIONS];

  struct memory_map map = {
      .available = available,
      .available_capacity = MAX_MEMORY_REGIONS,
      .reserved = reserved,
      .reserved_capacity = MAX_MEMORY_REGIONS,
  };
  int mem_map_init_res = ARCH_MEMORY_MAP_OK;
  if ((mem_map_init_res = arch_get_memory_map(device_discovery, &map)) !=
      ARCH_MEMORY_MAP_OK) {
    switch (mem_map_init_res) {
      case ARCH_MEMORY_MAP_INVALID:
        kpanic("Encountered invalid map while getting memory map.\n");
        break;
      case ARCH_MEMORY_MAP_NO_SPACE:
        kpanic("No space while getting memory map.\n");
        break;
      default:
        kpanic("Unexpected error while getting memory map.\n");
        break;
    }
  }

  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT,
              "Found %zu available memory regions.\n", map.available_count);

  for (size_t i = 0; i < map.available_count; i++) {
    struct memory_region region = map.available[i];
    console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT, "-> %p (%zu bytes)\n",
                (void*)region.offset, region.size);
  }

  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT,
              "Found %zu reserved memory regions.\n", map.reserved_count);

  for (size_t i = 0; i < map.reserved_count; i++) {
    struct memory_region region = map.reserved[i];
    console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT, "-> %p (%zu bytes)\n",
                (void*)region.offset, region.size);
  }

  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT, "Lowest address found:  %p.\n",
              (void*)map.min_phys_addr);
  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT, "Highest address found: %p.\n",
              (void*)map.max_phys_addr);

  struct memory_region window = arch_boot_alloc_window();
  boot_alloc_init(&map, window.offset, window.offset + window.size);

  int pmm_err;
  if ((pmm_err = pmm_init()) != PMM_OK) {
    switch (pmm_err) {
      case PMM_ERROR_INVALID_INPUT:
        kpanic("Invalid input while initialising PMM.");
        break;
      case PMM_ERROR_CANT_EMIT_REGION:
        kpanic("Failed while emitting regions during PMM initialisation.");
        break;
      default:
        kpanic("Unknown error while initialising PMM.");
        break;
    }
  }

  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT,
              "Initialised physical memory manager.\n");

  if (vmm_init() != 0) {
    kpanic("Failed to initialise virtual memory manager.");
  }

  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT,
              "Initialised virtual memory manager.\n");

  struct vmm_space* kernel_space = vmm_get_kernel_space();
  uintptr_t phys_test;

  // 1. Verify kmain virtual address translates to its physical address
  if (vmm_translate(kernel_space, (uintptr_t)kmain, &phys_test) != 0) {
    kpanic("Failed to translate kmain\n");
  }
  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT, "kmain translation: %p -> %p\n",
              (void*)kmain, (void*)phys_test);

  // 2. Verify HHDM virtual address translates to physical RAM
  uintptr_t hhdm_test_va = arch_phys_to_virt(0x80000000ULL);
  if (vmm_translate(kernel_space, hhdm_test_va, &phys_test) != 0 ||
      phys_test != 0x80000000ULL) {
    kpanic("Failed to translate HHDM RAM address\n");
  }
  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT, "HHDM translation: %p -> %p\n",
              (void*)hhdm_test_va, (void*)phys_test);

  // 3. Verify lower-half identity address 0x80000000 is NOT mapped in kernel_space
  if (vmm_translate(kernel_space, 0x80000000ULL, &phys_test) == 0) {
    kpanic("Lower-half identity address is still mapped!\n");
  }
  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT,
              "Lower-half address 0x80000000 is unmapped (verified).\n");

  // 4. Verify PMM allocation and access via HHDM works post-vmm_init
  uintptr_t test_frame;
  if (pmm_alloc(1, &test_frame) != PMM_OK) {
    kpanic("Failed to allocate frame post-vmm_init\n");
  }
  volatile uint64_t* frame_ptr =
      (volatile uint64_t*)arch_phys_to_virt(test_frame);
  *frame_ptr = 0xdeadbeefcafebabeULL;
  if (*frame_ptr != 0xdeadbeefcafebabeULL) {
    kpanic("HHDM write/read verification failed\n");
  }
  if (pmm_free(test_frame, 1) != PMM_OK) {
    kpanic("Failed to free frame post-vmm_init\n");
  }
  console_log(_CONSOLE_LOG_SUBJECT_MEM_INIT,
              "Post-VMM PMM alloc/write/free verified on frame %p.\n",
              (void*)test_frame);
}

void kmain(struct entry_info entry_info) {
  console_log(_CONSOLE_LOG_SUBJECT, "Entered kmain.\n");

  init_virtual_memory(entry_info.device_discovery);

  console_log(_CONSOLE_LOG_SUBJECT, "Reached end of kmain.\n");
  arch_panic_halt();
}
