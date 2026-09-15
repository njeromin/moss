#include <syscall/syscall.h>

#include <stddef.h>
#include <stdint.h>
#include <asm/trap_layout.h>

#include <core/console.h>

#define _CONSOLE_LOG_SUBJECT "syscall"

void syscall_handler(struct trap_frame* tf) {
  if (tf == NULL) {
    return;
  }

  uint64_t syscall_num = tf->a7;
  console_log(_CONSOLE_LOG_SUBJECT,
              "Syscall %zu received (a0=%p, a1=%p, a2=%p).\n",
              (size_t)syscall_num, (void*)tf->a0, (void*)tf->a1, (void*)tf->a2);

  tf->a0 = 0;
}
