#pragma once

#include <stddef.h>
#include <stdint.h>

// Core Lifecycle Syscalls
#define SYSCALL_YIELD 0x00
#define SYSCALL_EXIT 0x01

// Memory & Address Space Syscalls
#define SYSCALL_VSPACE_CREATE 0x10
#define SYSCALL_VSPACE_DESTROY 0x11
#define SYSCALL_VSPACE_MAP 0x12
#define SYSCALL_VSPACE_UNMAP 0x13
#define SYSCALL_FRAME_ALLOC 0x14
#define SYSCALL_FRAME_FREE 0x15

// Task Management Syscalls
#define SYSCALL_TASK_CREATE 0x20
#define SYSCALL_TASK_CONFIGURE 0x21
#define SYSCALL_TASK_START 0x22
#define SYSCALL_TASK_SUSPEND 0x23

// Inter-Process Communication Syscalls
#define SYSCALL_ENDPOINT_CREATE 0x30
#define SYSCALL_IPC_SEND 0x31
#define SYSCALL_IPC_RECV 0x32
#define SYSCALL_IPC_CALL 0x33
#define SYSCALL_IPC_REPLY 0x34

// Capability Management Syscalls
#define SYSCALL_CNODE_CREATE 0x40
#define SYSCALL_CAP_COPY 0x41
#define SYSCALL_CAP_DELETE 0x42

// Syscall Error Codes
enum syscall_error {
  SYS_OK = 0,
  SYS_ERR_INVALID_ARG = -1,
  SYS_ERR_INVALID_CAP = -2,
  SYS_ERR_RIGHTS = -3,
  SYS_ERR_NO_MEM = -4,
  SYS_ERR_ALREADY_MAPPED = -5,
  SYS_ERR_NO_SLOT = -6,
  SYS_ERR_NOT_FOUND = -7,
};

// Reserved Initial Capability Slots
#define CAP_SLOT_NULL 0
#define CAP_SLOT_VSPACE 1
#define CAP_SLOT_CNODE 2
#define CAP_SLOT_TASK 3

// Special slot specifier to request the next free slot
#define CAP_SLOT_ANY ((uint32_t)-1)

// Memory Mapping Protection Flags
#define PROT_NONE 0
#define PROT_READ (1u << 0)
#define PROT_WRITE (1u << 1)
#define PROT_EXEC (1u << 2)
#define PROT_USER (1u << 3)

