#pragma once

#include <common/syscall.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__riscv)

static inline long _syscall0(long num) {
  register long a7 __asm__("a7") = num;
  register long a0 __asm__("a0");
  __asm__ volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
  return a0;
}

static inline long _syscall1(long num, long a0) {
  register long a7 __asm__("a7") = num;
  register long r0 __asm__("a0") = a0;
  __asm__ volatile("ecall" : "+r"(r0) : "r"(a7) : "memory");
  return r0;
}

static inline long _syscall2(long num, long a0, long a1, long* out1) {
  register long a7 __asm__("a7") = num;
  register long r0 __asm__("a0") = a0;
  register long r1 __asm__("a1") = a1;
  __asm__ volatile("ecall" : "+r"(r0), "+r"(r1) : "r"(a7) : "memory");
  if (out1 != NULL) {
    *out1 = r1;
  }
  return r0;
}

static inline long _syscall3(long num, long a0, long a1, long a2) {
  register long a7 __asm__("a7") = num;
  register long r0 __asm__("a0") = a0;
  register long r1 __asm__("a1") = a1;
  register long r2 __asm__("a2") = a2;
  __asm__ volatile("ecall" : "+r"(r0), "+r"(r1), "+r"(r2) : "r"(a7) : "memory");
  return r0;
}

static inline long _syscall3_ret(long num, long a0, long a1, long a2,
                                 long* out1) {
  register long a7 __asm__("a7") = num;
  register long r0 __asm__("a0") = a0;
  register long r1 __asm__("a1") = a1;
  register long r2 __asm__("a2") = a2;
  __asm__ volatile("ecall"
                   : "+r"(r0), "+r"(r1), "+r"(r2)
                   : "r"(a7)
                   : "memory");
  if (out1 != NULL) {
    *out1 = r1;
  }
  return r0;
}

static inline long _syscall4(long num, long a0, long a1, long a2, long a3,
                             long* out1) {
  register long a7 __asm__("a7") = num;
  register long r0 __asm__("a0") = a0;
  register long r1 __asm__("a1") = a1;
  register long r2 __asm__("a2") = a2;
  register long r3 __asm__("a3") = a3;
  __asm__ volatile("ecall"
                   : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3)
                   : "r"(a7)
                   : "memory");
  if (out1 != NULL) {
    *out1 = r1;
  }
  return r0;
}

static inline long _syscall5(long num, long a0, long a1, long a2, long a3,
                             long a4, long* out1) {
  register long a7 __asm__("a7") = num;
  register long r0 __asm__("a0") = a0;
  register long r1 __asm__("a1") = a1;
  register long r2 __asm__("a2") = a2;
  register long r3 __asm__("a3") = a3;
  register long r4 __asm__("a4") = a4;
  __asm__ volatile("ecall"
                   : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3), "+r"(r4)
                   : "r"(a7)
                   : "memory");
  if (out1 != NULL) {
    *out1 = r1;
  }
  return r0;
}

#endif

// User-friendly wrappers

static inline void sys_yield(void) {
  _syscall0(SYSCALL_YIELD);
}

static inline _Noreturn void sys_exit(int code) {
  _syscall1(SYSCALL_EXIT, code);
  for (;;) {
  }
}

static inline int sys_frame_alloc(size_t count, uint32_t out_slot,
                                  uint32_t* assigned_slot) {
  long r1 = 0;
  long res = _syscall2(SYSCALL_FRAME_ALLOC, (long)count, (long)out_slot, &r1);
  if (res == SYS_OK && assigned_slot != NULL) {
    *assigned_slot = (uint32_t)r1;
  }
  return (int)res;
}

static inline int sys_frame_free(uint32_t frame_slot) {
  return (int)_syscall1(SYSCALL_FRAME_FREE, (long)frame_slot);
}

static inline int sys_vspace_create(uint32_t out_slot,
                                    uint32_t* assigned_slot) {
  long r1 = 0;
  long res = _syscall2(SYSCALL_VSPACE_CREATE, (long)out_slot, 0, &r1);
  if (res == SYS_OK && assigned_slot != NULL) {
    *assigned_slot = (uint32_t)r1;
  }
  return (int)res;
}

static inline int sys_vspace_destroy(uint32_t vspace_slot) {
  return (int)_syscall1(SYSCALL_VSPACE_DESTROY, (long)vspace_slot);
}

static inline int sys_vspace_map(uint32_t vspace_slot, uint32_t frame_slot,
                                 uintptr_t vaddr, uint32_t perms) {
  return (int)_syscall4(SYSCALL_VSPACE_MAP, (long)vspace_slot,
                        (long)frame_slot, (long)vaddr, (long)perms, NULL);
}

static inline int sys_vspace_unmap(uint32_t vspace_slot, uintptr_t vaddr,
                                   size_t size) {
  return (int)_syscall3(SYSCALL_VSPACE_UNMAP, (long)vspace_slot, (long)vaddr,
                        (long)size);
}

static inline int sys_task_create(uint32_t vspace_slot, uintptr_t entry,
                                  uintptr_t arg0, uint32_t out_slot,
                                  uint32_t* assigned_slot) {
  long r1 = 0;
  long res = _syscall4(SYSCALL_TASK_CREATE, (long)vspace_slot, (long)entry,
                        (long)arg0, (long)out_slot, &r1);
  if (res == SYS_OK && assigned_slot != NULL) {
    *assigned_slot = (uint32_t)r1;
  }
  return (int)res;
}

static inline int sys_task_configure(uint32_t task_slot, uintptr_t pc,
                                     uintptr_t sp, uintptr_t arg0) {
  return (int)_syscall4(SYSCALL_TASK_CONFIGURE, (long)task_slot, (long)pc,
                        (long)sp, (long)arg0, NULL);
}

static inline int sys_task_start(uint32_t task_slot, uint8_t priority) {
  return (int)_syscall2(SYSCALL_TASK_START, (long)task_slot, (long)priority,
                        NULL);
}

static inline int sys_task_suspend(uint32_t task_slot) {
  return (int)_syscall1(SYSCALL_TASK_SUSPEND, (long)task_slot);
}

static inline int sys_cnode_create(uint32_t vspace_slot, uint32_t out_slot,
                                   uint32_t* assigned_slot) {
  long r1 = 0;
  long res =
      _syscall2(SYSCALL_CNODE_CREATE, (long)vspace_slot, (long)out_slot, &r1);
  if (res == SYS_OK && assigned_slot != NULL) {
    *assigned_slot = (uint32_t)r1;
  }
  return (int)res;
}

static inline int sys_cap_copy(uint32_t src_slot, uint32_t dest_cnode_slot,
                               uint32_t dest_slot, uint8_t rights_mask,
                               uint64_t badge, uint32_t* assigned_slot) {
  long r1 = 0;
  long res = _syscall5(SYSCALL_CAP_COPY, (long)src_slot, (long)dest_cnode_slot,
                       (long)dest_slot, (long)rights_mask, (long)badge, &r1);
  if (res == SYS_OK && assigned_slot != NULL) {
    *assigned_slot = (uint32_t)r1;
  }
  return (int)res;
}

static inline int sys_cap_delete(uint32_t slot) {
  return (int)_syscall1(SYSCALL_CAP_DELETE, (long)slot);
}

static inline int sys_channel_create(uint32_t flags, uint32_t out_slot_a,
                                     uint32_t out_slot_b,
                                     uint32_t* assigned_slot_a,
                                     uint32_t* assigned_slot_b) {
  long r1 = 0;
  long res = _syscall3_ret(SYSCALL_CHANNEL_CREATE, (long)flags, (long)out_slot_a,
                           (long)out_slot_b, &r1);
  if (res == SYS_OK) {
    if (assigned_slot_a != NULL) {
      *assigned_slot_a = (uint32_t)(r1 & 0xffffffff);
    }
    if (assigned_slot_b != NULL) {
      *assigned_slot_b = (uint32_t)((r1 >> 32) & 0xffffffff);
    }
  }
  return (int)res;
}

static inline int sys_channel_write(uint32_t channel_slot, const void* data,
                                    size_t num_bytes, const uint32_t* caps,
                                    size_t num_caps) {
  return (int)_syscall5(SYSCALL_CHANNEL_WRITE, (long)channel_slot, (long)data,
                        (long)num_bytes, (long)caps, (long)num_caps, NULL);
}

static inline int sys_channel_read(uint32_t channel_slot, void* data_out,
                                   size_t max_bytes, size_t* actual_bytes,
                                   uint32_t* caps_out, size_t max_caps,
                                   size_t* actual_caps) {
  long r1 = 0;
  long res = _syscall5(SYSCALL_CHANNEL_READ, (long)channel_slot, (long)data_out,
                       (long)max_bytes, (long)caps_out, (long)max_caps, &r1);
  if (actual_bytes != NULL) {
    *actual_bytes = (size_t)(r1 & 0xffffffff);
  }
  if (actual_caps != NULL) {
    *actual_caps = (size_t)((r1 >> 32) & 0xffffffff);
  }
  return (int)res;
}

static inline int sys_object_wait_one(uint32_t obj_slot, uint32_t signals,
                                      uint64_t timeout_ns,
                                      uint32_t* observed_signals) {
  long r1 = 0;
  long res = _syscall3_ret(SYSCALL_OBJECT_WAIT_ONE, (long)obj_slot,
                           (long)signals, (long)timeout_ns, &r1);
  if (observed_signals != NULL) {
    *observed_signals = (uint32_t)r1;
  }
  return (int)res;
}
