#include <arch/timer.h>

#include <stddef.h>
#include <stdint.h>
#include <arch/cpu.h>
#include <arch/memory/layout.h>
#include <arch/trap.h>
#include <common/endian.h>
#include <common/fdt.h>
#include <core/console.h>
#include "sbi.h"

#define _CONSOLE_LOG_SUBJECT "timer"
#define SIE_STIE (1ULL << 5)
#define DEFAULT_TIMER_FREQ 10000000ULL /* 10 MHz default for RISC-V */

static uint64_t s_frequency = DEFAULT_TIMER_FREQ;
static bool s_has_sstc = false;
static bool s_initialised = false;
static arch_timer_callback_fn s_timer_callback = NULL;
static uint64_t s_timer_irq_count = 0;

static uint64_t dtb_find_timebase_frequency(const void* dtb) {
  if (dtb == NULL) {
    return 0;
  }

  struct fdt_node root;
  if (fdt_root(dtb, &root) != 0) {
    return 0;
  }

  struct fdt_node child;
  if (fdt_first_child(dtb, &root, &child) != 0) {
    return 0;
  }

  do {
    if (fdt_node_name_is(dtb, &child, "cpus")) {
      const void* data = NULL;
      size_t size = 0;

      // First check directly under /cpus
      if (fdt_get_property(dtb, &child, "timebase-frequency", &data, &size) ==
          0) {
        if (size == sizeof(uint32_t)) {
          return be32toh((const uint8_t*)data);
        } else if (size == sizeof(uint64_t)) {
          return be64toh((const uint8_t*)data);
        }
      }

      // Check /cpus/cpu@* children
      struct fdt_node cpu_child;
      if (fdt_first_child(dtb, &child, &cpu_child) == 0) {
        do {
          if (fdt_get_property(dtb, &cpu_child, "timebase-frequency", &data,
                               &size) == 0) {
            if (size == sizeof(uint32_t)) {
              return be32toh((const uint8_t*)data);
            } else if (size == sizeof(uint64_t)) {
              return be64toh((const uint8_t*)data);
            }
          }
        } while (fdt_next_sibling(dtb, &cpu_child, &cpu_child) == 0);
      }
    }
  } while (fdt_next_sibling(dtb, &child, &child) == 0);

  return 0;
}

static bool cpu_has_sstc(const void* dtb, const struct fdt_node* cpu) {
  const void* data;
  size_t size;
  if (fdt_get_property(dtb, cpu, "riscv,isa-extensions", &data, &size) == 0) {
    const char* text = data;
    for (size_t offset = 0; offset < size;) {
      size_t length = 0;
      while (length < size - offset && text[offset + length] != '\0') {
        ++length;
      }
      if (length == size - offset) {
        return false;
      }
      if (length == 4 && text[offset] == 's' && text[offset + 1] == 's' &&
          text[offset + 2] == 't' && text[offset + 3] == 'c') {
        return true;
      }
      offset += length + 1;
    }
  }
  const char* isa;
  if (fdt_property_string(dtb, cpu, "riscv,isa", &isa, &size) == 0) {
    for (size_t i = 1; i + 4 <= size; ++i) {
      if (isa[i - 1] == '_' && isa[i] == 's' && isa[i + 1] == 's' &&
          isa[i + 2] == 't' && isa[i + 3] == 'c' &&
          (i + 4 == size || isa[i + 4] == '_')) {
        return true;
      }
    }
  }
  return false;
}

static bool dtb_check_sstc(const void* dtb) {
  struct fdt_node root;
  struct fdt_node parent;
  if (dtb == NULL || fdt_root(dtb, &root) != 0 ||
      fdt_first_child(dtb, &root, &parent) != 0) {
    return false;
  }
  do {
    if (!fdt_node_name_is(dtb, &parent, "cpus")) {
      continue;
    }
    struct fdt_node cpu;
    bool found = false;
    if (fdt_first_child(dtb, &parent, &cpu) != 0) {
      return false;
    }
    do {
      if (!fdt_property_string_is(dtb, &cpu, "device_type", "cpu")) {
        continue;
      }
      const void* status;
      size_t size;
      if (fdt_get_property(dtb, &cpu, "status", &status, &size) == 0 &&
          !fdt_property_string_is(dtb, &cpu, "status", "okay") &&
          !fdt_property_string_is(dtb, &cpu, "status", "ok")) {
        continue;
      }
      found = true;
      // A shared backend must be supported by every enabled CPU.
      if (!cpu_has_sstc(dtb, &cpu)) {
        return false;
      }
    } while (fdt_next_sibling(dtb, &cpu, &cpu) == 0);
    return found;
  } while (fdt_next_sibling(dtb, &parent, &parent) == 0);
  return false;
}

int arch_timer_init(const struct device_discovery_info* discovery) {
  if (s_initialised) {
    return 0;
  }

  const void* dtb = NULL;
  if (discovery != NULL && discovery->dtb_phys != 0) {
    dtb = (discovery->dtb_phys >= HHDM_BASE)
              ? (const void*)discovery->dtb_phys
              : (const void*)arch_phys_to_virt(discovery->dtb_phys);
  }

  uint64_t freq = dtb_find_timebase_frequency(dtb);
  if (freq != 0) {
    s_frequency = freq;
  } else {
    s_frequency = DEFAULT_TIMER_FREQ;
  }

  s_has_sstc = dtb_check_sstc(dtb);

  arch_timer_init_cpu();

  console_log(_CONSOLE_LOG_SUBJECT,
              "Timer initialised: freq=%zu Hz, Sstc=%s.\n", (size_t)s_frequency,
              s_has_sstc ? "enabled (stimecmp)" : "disabled (using OpenSBI)");

  s_initialised = true;
  return 0;
}

void arch_timer_init_cpu(void) {
  arch_timer_disable_interrupt();
  arch_timer_cancel();
}

bool arch_timer_is_initialised(void) {
  return s_initialised;
}

bool arch_timer_has_direct(void) {
  return s_has_sstc;
}

uint64_t arch_timer_get_frequency(void) {
  return s_frequency;
}

uint64_t arch_timer_get_ticks(void) {
  uint64_t ticks;
  __asm__ volatile("rdtime %0" : "=r"(ticks));
  return ticks;
}

uint64_t arch_timer_ticks_to_nanos(uint64_t ticks) {
  uint64_t freq = s_frequency;
  if (freq == 0) {
    freq = DEFAULT_TIMER_FREQ;
  }
  uint64_t sec = ticks / freq;
  uint64_t rem = ticks % freq;
  return (sec * 1000000000ULL) + ((rem * 1000000000ULL) / freq);
}

uint64_t arch_timer_nanos_to_ticks(uint64_t nanos) {
  uint64_t freq = s_frequency;
  if (freq == 0) {
    freq = DEFAULT_TIMER_FREQ;
  }
  uint64_t sec = nanos / 1000000000ULL;
  uint64_t rem = nanos % 1000000000ULL;
  return (sec * freq) + ((rem * freq) / 1000000000ULL);
}

uint64_t arch_timer_get_nanos(void) {
  return arch_timer_ticks_to_nanos(arch_timer_get_ticks());
}

void arch_timer_set_deadline(uint64_t deadline_ticks) {
  if (s_has_sstc) {
    // CSR 0x14d = stimecmp
    __asm__ volatile("csrw 0x14d, %0" : : "r"(deadline_ticks) : "memory");
  } else {
    sbi_set_timer(deadline_ticks);
  }
}

void arch_timer_cancel(void) {
  if (s_has_sstc) {
    __asm__ volatile("csrw 0x14d, %0" : : "r"(~0ULL) : "memory");
  } else {
    sbi_set_timer(~0ULL);
  }
}

void arch_timer_enable_interrupt(void) {
  __asm__ volatile("csrs sie, %0" : : "r"(SIE_STIE) : "memory");
}

void arch_timer_disable_interrupt(void) {
  __asm__ volatile("csrc sie, %0" : : "r"(SIE_STIE) : "memory");
}

void arch_timer_set_callback(arch_timer_callback_fn callback) {
  __atomic_store_n(&s_timer_callback, callback, __ATOMIC_RELEASE);
}

void arch_timer_handle_interrupt(struct trap_frame* tf) {
  __atomic_fetch_add(&s_timer_irq_count, 1, __ATOMIC_RELAXED);
  arch_timer_callback_fn callback =
      __atomic_load_n(&s_timer_callback, __ATOMIC_ACQUIRE);
  if (callback != NULL) {
    callback(tf);
  } else {
    // Prevent re-triggering interrupt loop if no handler reprogrammed the deadline
    arch_timer_cancel();
  }
}

uint64_t arch_timer_get_irq_count(void) {
  return __atomic_load_n(&s_timer_irq_count, __ATOMIC_RELAXED);
}

void arch_delay_ticks(uint64_t ticks) {
  uint64_t start = arch_timer_get_ticks();
  while ((arch_timer_get_ticks() - start) < ticks) {
    arch_cpu_relax();
  }
}

void arch_delay_us(uint64_t us) {
  arch_delay_ticks(arch_timer_nanos_to_ticks(us * 1000ULL));
}

void arch_delay_ms(uint64_t ms) {
  arch_delay_ticks(arch_timer_nanos_to_ticks(ms * 1000000ULL));
}
