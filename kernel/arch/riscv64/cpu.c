#include <arch/cpu.h>
#include <asm/hw_features.h>  // Needed as this implements arch_has_svadu()

#include <arch/trap.h>
#include <core/console.h>
#include <core/panic.h>
#include "sbi.h"

#define _CONSOLE_LOG_SUBJECT "cpu"

struct cpu_features {
  bool probed;
  bool has_hsm;
  bool has_fwft;
  bool svadu_enabled;
  bool double_trap_enabled;
  bool landing_pad_enabled;
};

static struct cpu_features features;

static void configure_cpu_features(void) {
  if (!features.has_fwft) {
    return;
  }

  // 1. Hardware PTE A/D Updating (Svadu)
  if (!features.probed) {
    struct sbi_ret ret = sbi_fwft_set(SBI_FWFT_PTE_AD_HW_UPDATING, 1, 0);
    if (ret.error == SBI_SUCCESS) {
      struct sbi_ret get_val = sbi_fwft_get(SBI_FWFT_PTE_AD_HW_UPDATING);
      if (get_val.error == SBI_SUCCESS && get_val.value == 1) {
        features.svadu_enabled = true;
        console_log(_CONSOLE_LOG_SUBJECT,
                    "Enabled Svadu (hardware PTE A/D updating).\n");
      }
    }
    if (!features.svadu_enabled) {
      console_log(_CONSOLE_LOG_SUBJECT,
                  "Svadu not available (software A/D updating).\n");
    }
  } else if (features.svadu_enabled) {
    struct sbi_ret ret = sbi_fwft_set(SBI_FWFT_PTE_AD_HW_UPDATING, 1, 0);
    if (ret.error != SBI_SUCCESS) {
      kpanic("Secondary CPU cannot support the kernel A/D update policy.\n");
    }
  }

  // Misaligned Exception Delegation
  struct sbi_ret ret = sbi_fwft_set(SBI_FWFT_MISALIGNED_EXC_DELEG, 1, 0);
  if (!features.probed && ret.error == SBI_SUCCESS) {
    console_log(_CONSOLE_LOG_SUBJECT,
                "Delegated misaligned access exceptions to S-mode.\n");
  }

  // Double Trap Handling
  if (!features.probed) {
    ret = sbi_fwft_set(SBI_FWFT_DOUBLE_TRAP, 1, 0);
    if (ret.error == SBI_SUCCESS) {
      features.double_trap_enabled = true;
      console_log(_CONSOLE_LOG_SUBJECT, "Enabled double trap prevention.\n");
    } else {
      console_log(_CONSOLE_LOG_SUBJECT,
                  "Double trap prevention not supported (error=%d).\n",
                  (int)ret.error);
    }
  } else if (features.double_trap_enabled) {
    sbi_fwft_set(SBI_FWFT_DOUBLE_TRAP, 1, 0);
  }

  // Landing Pad (Zicfilp)
  if (!features.probed) {
    ret = sbi_fwft_set(SBI_FWFT_LANDING_PAD, 1, 0);
    if (ret.error == SBI_SUCCESS) {
      features.landing_pad_enabled = true;
      console_log(_CONSOLE_LOG_SUBJECT, "Enabled Landing Pad (Zicfilp).\n");
    } else {
      console_log(_CONSOLE_LOG_SUBJECT,
                  "Landing Pad not supported (error=%d).\n", (int)ret.error);
    }
  } else if (features.landing_pad_enabled) {
    sbi_fwft_set(SBI_FWFT_LANDING_PAD, 1, 0);
  }

  if (features.landing_pad_enabled) {
    __asm__ volatile("csrs senvcfg, %0" ::"r"(1ULL << 2));
    if (!features.probed) {
      console_log(_CONSOLE_LOG_SUBJECT,
                  "Enabled U-mode landing pad enforcement (senvcfg.LPE).\n");
    }
  }
}

void arch_cpu_init(cpu_hardware_id_t hart_id) {
  (void)hart_id;

  // Local trap vectors must be configured on every CPU
  arch_trap_init();

  if (!features.probed) {
    struct sbi_ret hsm_probe = sbi_probe_extension(SBI_EXT_HSM);
    features.has_hsm = (hsm_probe.error == SBI_SUCCESS && hsm_probe.value != 0);

    struct sbi_ret fwft_probe = sbi_probe_extension(SBI_EXT_FWFT);
    features.has_fwft =
        (fwft_probe.error == SBI_SUCCESS && fwft_probe.value != 0);
    if (!features.has_fwft) {
      console_log(_CONSOLE_LOG_SUBJECT, "SBI FWFT extension not available.\n");
    }
  }

  configure_cpu_features();

  features.probed = true;
}

bool arch_has_svadu(void) {
  return features.svadu_enabled;
}

bool arch_has_landing_pad(void) {
  return features.landing_pad_enabled;
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
  if (features.has_hsm) {
    struct sbi_ret ret = sbi_hart_suspend(SBI_HART_SUSPEND_RETENTIVE, 0, 0);
    if (ret.error == SBI_SUCCESS) {
      if (!arch_interrupts_enabled()) {
        __asm__ volatile("csrsi sstatus, 2\nnop\ncsrci sstatus, 2" ::
                             : "memory");
      }
      return;
    }
    if (ret.error == SBI_ERR_NOT_SUPPORTED) {
      features.has_hsm = false;
    }
  }

  // Pending enabled interrupts wake WFI even with global SIE clear.
  __asm__ volatile("wfi\ncsrsi sstatus, 2\nnop\ncsrci sstatus, 2" ::: "memory");
}
