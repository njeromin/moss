#include <syscall/syscall.h>

#include <stddef.h>
#include <stdint.h>
#include <asm/trap_layout.h>

#include <core/console.h>
#include <sched/sched.h>

#define _CONSOLE_LOG_SUBJECT "syscall"

void syscall_handler(size_t syscall_num,
                     const trap_register_t args[TRAP_ARG_REGISTER_COUNT],
                     trap_register_t* ret0, trap_register_t* ret1) {
  (void)args;
  (void)ret0;
  (void)ret1;

  switch (syscall_num) {
    case SYSCALL_YIELD:
      sched_preempt();
      break;
    case SYSCALL_EXIT:
      sched_exit((int)args[0]);
      break;
    default:
      console_log(_CONSOLE_LOG_SUBJECT, "Unexpected syscall number %zu.\n",
                  syscall_num);
      break;
  }
}
