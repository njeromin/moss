#pragma once

#include <asm/trap_layout.h>

#define SYSCALL_YIELD 0x0
#define SYSCALL_EXIT 0x1

// Handle system calls invoked from user mode.
void syscall_handler(size_t syscall_num,
                     const trap_register_t args[TRAP_ARG_REGISTER_COUNT],
                     trap_register_t* ret0, trap_register_t* ret1);
