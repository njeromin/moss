#include <arch/timer.h>
#include <arch/trap.h>
#include <core/console.h>
#include <core/panic.h>
#include <sched/sched.h>

#define _CONSOLE_LOG_SUBJECT "trap"

static const char* exception_name(uint64_t code) {
  switch (code) {
    case 0:
      return "Instruction address misaligned";
    case 1:
      return "Instruction access fault";
    case 2:
      return "Illegal instruction";
    case 3:
      return "Breakpoint";
    case 4:
      return "Load address misaligned";
    case 5:
      return "Load access fault";
    case 6:
      return "Store/AMO address misaligned";
    case 7:
      return "Store/AMO access fault";
    case 8:
      return "Environment call from U-mode";
    case 9:
      return "Environment call from S-mode";
    case 12:
      return "Instruction page fault";
    case 13:
      return "Load page fault";
    case 15:
      return "Store/AMO page fault";
    case 18:
      return "Software check fault";
    case 19:
      return "Hardware error";
    default:
      return "Unknown exception";
  }
}

static const char* interrupt_name(uint64_t code) {
  switch (code) {
    case 1:
      return "Supervisor software interrupt";
    case 5:
      return "Supervisor timer interrupt";
    case 9:
      return "Supervisor external interrupt";
    default:
      return "Unknown interrupt";
  }
}

static void dump_trap_frame(const struct trap_frame* tf,
                            const char* description, bool is_interrupt,
                            uint64_t code) {
  console_write("\n==================== KERNEL TRAP ====================\n");
  console_write("Type:    %s (code %zu): %s\n",
                is_interrupt ? "Interrupt" : "Exception", (size_t)code,
                description);
  console_write("sepc:    %p  stval:   %p\n", (void*)tf->sepc,
                (void*)tf->stval);
  console_write("scause:  %p  sstatus: %p\n", (void*)tf->scause,
                (void*)tf->sstatus);
  console_write("Registers:\n");
  console_write("  ra:  %p  sp:  %p  gp:  %p  tp:  %p\n", (void*)tf->ra,
                (void*)tf->sp, (void*)tf->gp, (void*)tf->tp);
  console_write("  t0:  %p  t1:  %p  t2:  %p  s0:  %p\n", (void*)tf->t0,
                (void*)tf->t1, (void*)tf->t2, (void*)tf->s0);
  console_write("  s1:  %p  a0:  %p  a1:  %p  a2:  %p\n", (void*)tf->s1,
                (void*)tf->a0, (void*)tf->a1, (void*)tf->a2);
  console_write("  a3:  %p  a4:  %p  a5:  %p  a6:  %p\n", (void*)tf->a3,
                (void*)tf->a4, (void*)tf->a5, (void*)tf->a6);
  console_write("  a7:  %p  s2:  %p  s3:  %p  s4:  %p\n", (void*)tf->a7,
                (void*)tf->s2, (void*)tf->s3, (void*)tf->s4);
  console_write("  s5:  %p  s6:  %p  s7:  %p  s8:  %p\n", (void*)tf->s5,
                (void*)tf->s6, (void*)tf->s7, (void*)tf->s8);
  console_write("  s9:  %p  s10: %p  s11: %p  t3:  %p\n", (void*)tf->s9,
                (void*)tf->s10, (void*)tf->s11, (void*)tf->t3);
  console_write("  t4:  %p  t5:  %p  t6:  %p\n", (void*)tf->t4, (void*)tf->t5,
                (void*)tf->t6);
  console_write("=====================================================\n");
}

#define SSTATUS_SPP (1ULL << 8)

bool arch_trap_is_usermode(const struct trap_frame* tf) {
  return tf != NULL && (tf->sstatus & SSTATUS_SPP) == 0;
}

void arch_trap_init(void) {
  uintptr_t stvec = (uintptr_t)arch_trap_entry;
  // Set stvec to direct mode (lowest 2 bits 0)
  __asm__ volatile("csrw stvec, %0" : : "r"(stvec) : "memory");
  // In S-mode kernel execution, sscratch is 0
  __asm__ volatile("csrw sscratch, zero" ::: "memory");

  // Enable supervisor software interrupts (SSIE) for reschedule IPIs
  __asm__ volatile("csrs sie, %0" : : "r"(1ULL << 1) : "memory");

  console_log(_CONSOLE_LOG_SUBJECT, "Trap vector initialised (stvec=%p).\n",
              (void*)stvec);
}

void arch_trap_handler(struct trap_frame* tf) {
  bool is_interrupt = ((int64_t)tf->scause < 0);
  uint64_t code = tf->scause & ~(1ULL << 63);

  if (is_interrupt) {
    if (code == 1) {
      // Clear supervisor software interrupt pending bit
      __asm__ volatile("csrc sip, %0" : : "r"(1ULL << 1) : "memory");
      if (arch_trap_is_usermode(tf)) {
        sched_preempt(tf);
      }
      return;
    }

    if (code == 5) {
      arch_timer_handle_interrupt(tf);
      if (arch_trap_is_usermode(tf)) {
        sched_preempt(tf);
      }
      return;
    }

    const char* name = interrupt_name(code);
    console_log(_CONSOLE_LOG_SUBJECT, "Unhandled interrupt: %s (%zu)\n", name,
                (size_t)code);
    dump_trap_frame(tf, name, true, code);
    kpanic("Unhandled kernel interrupt\n");
  }

  // Synchronous Exceptions
  if (code == 3) {
    // Breakpoint exception (ebreak)
    console_log(_CONSOLE_LOG_SUBJECT, "Breakpoint caught at %p (resuming).\n",
                (void*)tf->sepc);
    uint64_t sstatus_sum = (1ULL << 18);
    __asm__ volatile("csrs sstatus, %0" : : "r"(sstatus_sum) : "memory");
    uint16_t instr = *(const uint16_t*)tf->sepc;
    __asm__ volatile("csrc sstatus, %0" : : "r"(sstatus_sum) : "memory");
    tf->sepc += ((instr & 0x3) == 0x3) ? 4 : 2;
    return;
  }

  const char* name = exception_name(code);
  dump_trap_frame(tf, name, false, code);
  kpanic("Unhandled kernel exception: %s (code %zu) at %p\n", name,
         (size_t)code, (void*)tf->sepc);
}
