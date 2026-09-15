#pragma once

#include <asm/trap_layout.h>

// Handle system calls invoked from user mode.
void syscall_handler(struct trap_frame* tf);
