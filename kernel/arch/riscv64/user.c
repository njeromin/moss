#include <arch/user.h>

#include <common/memory.h>

#define SSTATUS_SPP (1ULL << 8)   // 0 = User mode, 1 = Supervisor mode
#define SSTATUS_SPIE (1ULL << 5)  // Previous interrupt enable

void arch_user_context_init(struct trap_frame* tf, uintptr_t entry_point,
                            uintptr_t user_sp, uintptr_t arg0) {
  memory_set(tf, 0, sizeof(*tf));

  tf->sepc = entry_point;
  tf->sp = user_sp;
  tf->a0 = arg0;

  // sstatus.SPP = 0 (return to U-mode), sstatus.SPIE = 1 (enable interrupts after sret)
  tf->sstatus = SSTATUS_SPIE;
}

void arch_user_context_set_tls(struct trap_frame* tf, uintptr_t tls_base) {
  tf->tp = tls_base;
}
