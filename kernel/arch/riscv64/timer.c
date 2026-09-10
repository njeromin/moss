#include <arch/timer.h>

#include "sbi.h"
#include <arch/cpu_relax.h>
#include <arch/memory/layout.h>
#include <arch/trap.h>
#include <common/endian.h>
#include <common/fdt.h>
#include <core/console.h>
#include <stddef.h>
#include <stdint.h>

#define _CONSOLE_LOG_SUBJECT "timer"
#define SIE_STIE (1ULL << 5)
#define DEFAULT_TIMER_FREQ 10000000ULL /* 10 MHz default for RISC-V */

static uint64_t s_frequency = DEFAULT_TIMER_FREQ;
static bool s_has_sstc = false;
static bool s_initialised = false;
static arch_timer_callback_fn s_timer_callback = NULL;
static volatile uint64_t s_timer_irq_count = 0;

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

static bool dtb_check_sstc(const void* dtb) {
  if (dtb == NULL) {
    return false;
  }

  struct fdt_node root;
  if (fdt_root(dtb, &root) != 0) {
    return false;
  }

  struct fdt_node child;
  if (fdt_first_child(dtb, &root, &child) != 0) {
    return false;
  }

  do {
    if (fdt_node_name_is(dtb, &child, "cpus")) {
      struct fdt_node cpu_child;
      if (fdt_first_child(dtb, &child, &cpu_child) == 0) {
        do {
          const void* data = NULL;
          size_t size = 0;

          // Check riscv,isa-extensions (null-separated string list)
          if (fdt_get_property(dtb, &cpu_child, "riscv,isa-extensions", &data,
                               &size) == 0) {
            const char* s = (const char*)data;
            size_t offset = 0;
            while (offset < size) {
              size_t len = 0;
              while (offset + len < size && s[offset + len] != '\0') {
                len++;
              }
              if (len == 4 && s[offset] == 's' && s[offset + 1] == 's' &&
                  s[offset + 2] == 't' && s[offset + 3] == 'c') {
                return true;
              }
              offset += len + 1;
            }
          }

          // Check riscv,isa string
          if (fdt_get_property(dtb, &cpu_child, "riscv,isa", &data, &size) ==
              0) {
            const char* s = (const char*)data;
            if (size >= 4) {
              for (size_t i = 0; i <= size - 4; i++) {
                if (s[i] == 's' && s[i + 1] == 's' && s[i + 2] == 't' &&
                    s[i + 3] == 'c') {
                  char prev = (i > 0) ? s[i - 1] : '_';
                  char next = (i + 4 < size) ? s[i + 4] : '\0';
                  if ((prev == '_' || prev == ',') &&
                      (next == '_' || next == '\0' || next == ',')) {
                    return true;
                  }
                }
              }
            }
          }
        } while (fdt_next_sibling(dtb, &cpu_child, &cpu_child) == 0);
      }
    }
  } while (fdt_next_sibling(dtb, &child, &child) == 0);

  return false;
}

int arch_timer_init(const struct device_discovery_info* discovery) {
  if (s_initialised) {
    return 0;
  }

  const void* dtb = NULL;
  if (discovery != NULL && discovery->dtb != 0) {
    dtb = (discovery->dtb >= HHDM_BASE)
              ? (const void*)discovery->dtb
              : (const void*)arch_phys_to_virt(discovery->dtb);
  }

  uint64_t freq = dtb_find_timebase_frequency(dtb);
  if (freq != 0) {
    s_frequency = freq;
  } else {
    s_frequency = DEFAULT_TIMER_FREQ;
  }

  s_has_sstc = dtb_check_sstc(dtb);

  // Disarm any pending timer interrupts
  arch_timer_cancel();

  console_log(_CONSOLE_LOG_SUBJECT,
              "Timer initialised: freq=%zu Hz, Sstc=%s.\n", (size_t)s_frequency,
              s_has_sstc ? "enabled (stimecmp)" : "disabled (using OpenSBI)");

  s_initialised = true;
  return 0;
}

bool arch_timer_is_initialised(void) {
  return s_initialised;
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
  s_timer_callback = callback;
}

void arch_timer_handle_interrupt(struct trap_frame* tf) {
  s_timer_irq_count++;
  if (s_timer_callback != NULL) {
    s_timer_callback(tf);
  } else {
    // Prevent re-triggering interrupt loop if no handler reprogrammed the deadline
    arch_timer_cancel();
  }
}

uint64_t arch_timer_get_irq_count(void) {
  return s_timer_irq_count;
}

void arch_delay_ticks(uint64_t ticks) {
  uint64_t start = arch_timer_get_ticks();
  while ((arch_timer_get_ticks() - start) < ticks) {
    arch_cpu_relax();
    __asm__ volatile("nop");
  }
}

void arch_delay_us(uint64_t us) {
  arch_delay_ticks(arch_timer_nanos_to_ticks(us * 1000ULL));
}

void arch_delay_ms(uint64_t ms) {
  arch_delay_ticks(arch_timer_nanos_to_ticks(ms * 1000000ULL));
}
