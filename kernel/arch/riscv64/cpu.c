#include <arch/cpu.h>
#include <arch/trap.h>
#include <core/console.h>
#include "sbi.h"

#define _CONSOLE_LOG_SUBJECT "cpu"

static bool svadu_enabled = false;

void arch_cpu_init(size_t hart_id) {
  (void)hart_id;

  arch_trap_init();

  struct sbi_ret probe = sbi_probe_extension(SBI_EXT_FWFT);
  if (probe.error == SBI_SUCCESS && probe.value != 0) {
    struct sbi_ret ret = sbi_fwft_set(SBI_FWFT_PTE_AD_HW_UPDATING, 1, 0);
    if (ret.error == SBI_SUCCESS) {
      struct sbi_ret get_val = sbi_fwft_get(SBI_FWFT_PTE_AD_HW_UPDATING);
      if (get_val.error == SBI_SUCCESS && get_val.value == 1) {
        svadu_enabled = true;
        console_log(_CONSOLE_LOG_SUBJECT,
                    "Enabled Svadu (hardware PTE A/D updating).\n");
        return;
      }
    }
  }

  console_log(_CONSOLE_LOG_SUBJECT,
              "Svadu not available (software A/D updating).\n");
}

bool arch_has_svadu(void) {
  return svadu_enabled;
}

#define SSTATUS_SIE (1ULL << 1)

void arch_interrupts_enable(void) {
  __asm__ volatile("csrs sstatus, %0" : : "r"(SSTATUS_SIE) : "memory");
}

void arch_interrupts_disable(void) {
  __asm__ volatile("csrc sstatus, %0" : : "r"(SSTATUS_SIE) : "memory");
}

bool arch_interrupts_enabled(void) {
  uint64_t sstatus;
  __asm__ volatile("csrr %0, sstatus" : "=r"(sstatus));
  return (sstatus & SSTATUS_SIE) != 0;
}
