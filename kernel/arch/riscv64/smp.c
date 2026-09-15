#include <arch/smp.h>

#include <arch/memory/layout.h>
#include <arch/timer.h>
#include <core/panic.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include "sbi.h"

struct secondary_startup {
  uintptr_t satp;
  uintptr_t sp;
  uintptr_t entry;
  size_t logical_id;
  void (*callback)(size_t);
};

_Static_assert(offsetof(struct secondary_startup, satp) == 0, "satp offset");
_Static_assert(offsetof(struct secondary_startup, sp) == 8, "sp offset");
_Static_assert(offsetof(struct secondary_startup, entry) == 16, "entry offset");
_Static_assert(offsetof(struct secondary_startup, logical_id) == 24,
               "ID offset");

extern const uintptr_t secondary_entry_address;
static bool trampoline_mapped;

// Reached after paging is enabled. Establish gp before entering compiled C.
__attribute__((naked, noreturn)) static void secondary_high_entry(void) {
  __asm__ volatile(".option push\n"
                   ".option norelax\n"
                   "la gp, __global_pointer$\n"
                   ".option pop\n"
                   "tail secondary_cpu_entry\n");
}

__attribute__((used, noreturn)) static void
secondary_cpu_entry(cpu_hardware_id_t hardware_id, uintptr_t startup_phys) {
  struct secondary_startup* startup =
      (struct secondary_startup*)arch_phys_to_virt(startup_phys);
  void (*callback)(size_t) = startup->callback;
  size_t id = startup->logical_id;
  arch_cpu_init(hardware_id);
  arch_timer_init_cpu();
  // The assembly no longer uses the startup block, and all values are local.
  if (pmm_free(startup_phys, 1) != PMM_OK) {
    kpanic("Failed to release secondary startup storage.\n");
  }
  callback(id);
  kpanic("Secondary CPU entry returned.\n");
}

int arch_smp_start_cpu(cpu_hardware_id_t hardware_id,
                       const struct cpu_start_info* info) {
  if (!trampoline_mapped) {
    if (vmm_map(vmm_get_kernel_space(), secondary_entry_address,
                secondary_entry_address, PMM_FRAME_SIZE,
                PAGE_READ | PAGE_EXEC) != VMM_OK) {
      return -1;
    }
    trampoline_mapped = true;
  }

  uintptr_t phys;
  if (pmm_alloc(1, &phys) != PMM_OK) {
    return -1;
  }
  struct secondary_startup* startup =
      (struct secondary_startup*)arch_phys_to_virt(phys);
  *startup = (struct secondary_startup){
      .satp = (8ULL << 60) | (vmm_get_kernel_space()->root_phys >> 12),
      .sp = info->stack_top,
      .entry = (uintptr_t)secondary_high_entry,
      .logical_id = info->logical_id,
      .callback = info->entry,
  };
  __atomic_thread_fence(__ATOMIC_RELEASE);
  struct sbi_ret result =
      sbi_hart_start(hardware_id, secondary_entry_address, phys);
  if (result.error != SBI_SUCCESS) {
    pmm_free(phys, 1);
    return -1;
  }
  return 0;
}

void arch_smp_finish_startup(void) {
  if (trampoline_mapped) {
    if (vmm_unmap(vmm_get_kernel_space(), secondary_entry_address,
                  PMM_FRAME_SIZE) != VMM_OK) {
      kpanic("Failed to remove secondary trampoline mapping.\n");
    }
    trampoline_mapped = false;
  }
}

void arch_cpu_set_logical_id(size_t id) {
  __asm__ volatile("mv tp, %0" : : "r"(id) : "memory");
}

size_t arch_cpu_logical_id(void) {
  size_t id;
  __asm__ volatile("mv %0, tp" : "=r"(id));
  return id;
}

void arch_smp_reschedule_cpu(cpu_hardware_id_t hardware_id) {
  if (sbi_send_ipi(1, hardware_id).error != SBI_SUCCESS) {
    kpanic("Failed to send scheduler IPI.\n");
  }
}
