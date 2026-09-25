#pragma once

#include <asm/trap_layout.h>
#include <common/syscall.h>

// Handle system calls invoked from user mode.
void syscall_handler(size_t syscall_num,
                     const trap_register_t args[TRAP_ARG_REGISTER_COUNT],
                     trap_register_t* ret0, trap_register_t* ret1);
