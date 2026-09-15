#include <arch/cpu.h>
#include <asm/hw_features.h>  // Needed as this implements arch_has_svadu()

#include <arch/trap.h>
#include <core/console.h>
#include <core/panic.h>
#include "sbi.h"

#define _CONSOLE_LOG_SUBJECT "cpu"

static bool svadu_enabled = false;
static bool features_initialised;

void arch_cpu_init(cpu_hardware_id_t hart_id) {
  (void)hart_id;

  arch_trap_init();
  bool local_svadu = false;

  struct sbi_ret probe = sbi_probe_extension(SBI_EXT_FWFT);
  if (probe.error == SBI_SUCCESS && probe.value != 0) {
    // Hardware PTE A/D Updating (Svadu)
    struct sbi_ret ret = sbi_fwft_set(SBI_FWFT_PTE_AD_HW_UPDATING, 1, 0);
    if (ret.error == SBI_SUCCESS) {
      struct sbi_ret get_val = sbi_fwft_get(SBI_FWFT_PTE_AD_HW_UPDATING);
      if (get_val.error == SBI_SUCCESS && get_val.value == 1) {
        local_svadu = true;
        console_log(_CONSOLE_LOG_SUBJECT,
                    "Enabled Svadu (hardware PTE A/D updating).\n");
      }
    }
    if (!local_svadu) {
      console_log(_CONSOLE_LOG_SUBJECT,
                  "Svadu not available (software A/D updating).\n");
    }

    // Misaligned Exception Delegation
    ret = sbi_fwft_set(SBI_FWFT_MISALIGNED_EXC_DELEG, 1, 0);
    if (ret.error == SBI_SUCCESS) {
      console_log(_CONSOLE_LOG_SUBJECT,
                  "Delegated misaligned access exceptions to S-mode.\n");
    }

    // Double Trap Handling
    ret = sbi_fwft_set(SBI_FWFT_DOUBLE_TRAP, 1, 0);
    if (ret.error == SBI_SUCCESS) {
      console_log(_CONSOLE_LOG_SUBJECT, "Enabled double trap prevention.\n");
    } else {
      console_log(_CONSOLE_LOG_SUBJECT,
                  "Double trap prevention not supported (error=%d).\n",
                  (int)ret.error);
    }
  } else {
    console_log(_CONSOLE_LOG_SUBJECT, "SBI FWFT extension not available.\n");
  }
  // Boot CPU establishes the paging policy before serial secondary startup.
  // Never change that policy underneath existing page tables.
  if (!features_initialised) {
    svadu_enabled = local_svadu;
    features_initialised = true;
  } else if (svadu_enabled && !local_svadu) {
    kpanic("Secondary CPU cannot support the kernel A/D update policy.\n");
  }
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

void arch_cpu_idle(void) {
  // Pending enabled interrupts wake WFI even with global SIE clear.
  __asm__ volatile("wfi\ncsrsi sstatus, 2\nnop\ncsrci sstatus, 2" ::: "memory");
}
