#pragma once

#include <arch/cpu.h>
#include <stdbool.h>

// Zero-initialised, non-recursive lock. Interrupt masking prevents a local
// interrupt handler from spinning on a lock held by the interrupted context.
struct spinlock {
  unsigned held;
};

static inline bool spin_lock_irqsave(struct spinlock* lock) {
  bool enabled = arch_interrupts_enabled();
  arch_interrupts_disable();
  while (__atomic_exchange_n(&lock->held, 1, __ATOMIC_ACQUIRE)) {
    while (__atomic_load_n(&lock->held, __ATOMIC_RELAXED)) {
      __asm__ volatile("" ::: "memory");
    }
  }
  return enabled;
}

static inline void spin_unlock_irqrestore(struct spinlock* lock, bool enabled) {
  __atomic_store_n(&lock->held, 0, __ATOMIC_RELEASE);
  if (enabled) {
    arch_interrupts_enable();
  }
}
