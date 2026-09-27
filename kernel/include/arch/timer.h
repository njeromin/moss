#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <arch/trap.h>
#include <core/kmain.h>

// Callback function type for hardware timer tick events.
typedef void (*arch_timer_callback_fn)(struct trap_frame* tf);

/*
 * Initialise the architecture timer subsystem.
 * Discovers clock frequency and available timer extensions from device discovery / DTB.
 */
int arch_timer_init(const struct device_discovery_info* discovery);

// Reset the executing CPU's timer after global timer discovery has completed.
void arch_timer_init_cpu(void);

// Returns true if the timer has been initialised.
bool arch_timer_is_initialised(void);

// Returns true if the timer supports direct architectural access without firmware trapping.
bool arch_timer_has_direct(void);

// Returns the timer frequency in Hz (ticks per second).
uint64_t arch_timer_get_frequency(void);

// Read current monotonic cycle counter / ticks.
uint64_t arch_timer_get_ticks(void);

// Read current time in nanoseconds since boot.
uint64_t arch_timer_get_nanos(void);

// Conversion between ticks and nanoseconds.
uint64_t arch_timer_ticks_to_nanos(uint64_t ticks);
uint64_t arch_timer_nanos_to_ticks(uint64_t nanos);

// Arm the hardware timer to fire an interrupt at the specified future monotonic tick.
void arch_timer_set_deadline(uint64_t deadline_ticks);

// Cancel/disarm the timer interrupt.
void arch_timer_cancel(void);

// Enable/disable timer interrupts on the current CPU core (sie.STIE on RISC-V).
void arch_timer_enable_interrupt(void);
void arch_timer_disable_interrupt(void);

// Register a callback to be called on supervisor timer interrupt.
void arch_timer_set_callback(arch_timer_callback_fn callback);

// Low-level trap dispatcher hook called on supervisor timer interrupt.
void arch_timer_handle_interrupt(struct trap_frame* tf);

// Returns the total number of timer interrupts handled.
uint64_t arch_timer_get_irq_count(void);

// Busy-wait delays.
void arch_delay_ticks(uint64_t ticks);
void arch_delay_us(uint64_t us);
void arch_delay_ms(uint64_t ms);
