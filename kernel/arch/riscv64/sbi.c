#include "sbi.h"

#define SBI_EID_BASE 0x10
#define SBI_EID_TIMER 0x54494D45
#define SBI_EID_CONSOLE 0x4442434E

#define SBI_FID_GET_SPEC_VERSION 0

#define SBI_FID_SET_TIMER 0

#define SBI_FID_CONSOLE_WRITE 0
#define SBI_FID_CONSOLE_READ 1
#define SBI_FID_CONSOLE_WRITE_BYTE 2

static struct sbi_ret sbi_ecall(long eid, long fid, long arg0, long arg1,
                                long arg2, long arg3, long arg4, long arg5) {
  register long a0 __asm__("a0") = arg0;
  register long a1 __asm__("a1") = arg1;
  register long a2 __asm__("a2") = arg2;
  register long a3 __asm__("a3") = arg3;
  register long a4 __asm__("a4") = arg4;
  register long a5 __asm__("a5") = arg5;
  register long a6 __asm__("a6") = fid;
  register long a7 __asm__("a7") = eid;

  // SBI v0.2+: a0/a1 return error/value; all other registers are preserved.
  // The memory clobber prevents reordering accesses to SBI shared buffers.
  __asm__ volatile("ecall"
                   : "+r"(a0), "+r"(a1)
                   : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
                   : "memory");

  return (struct sbi_ret){.error = a0, .value = a1};
}

/* Base Extension */

struct sbi_ret sbi_get_spec_version(void) {
  return sbi_ecall(SBI_EID_BASE, SBI_FID_GET_SPEC_VERSION, 0, 0, 0, 0, 0, 0);
}

/* Timer Extension */

struct sbi_ret sbi_set_timer(uint64_t stime_value) {
  return sbi_ecall(SBI_EID_TIMER, SBI_FID_SET_TIMER, stime_value, 0, 0, 0, 0,
                   0);
}

/* Console Extension */

struct sbi_ret sbi_debug_console_write(unsigned long num_bytes,
                                       unsigned long base_addr_lo,
                                       unsigned long base_addr_hi) {
  return sbi_ecall(SBI_EID_CONSOLE, SBI_FID_CONSOLE_WRITE, num_bytes,
                   base_addr_lo, base_addr_hi, 0, 0, 0);
}

struct sbi_ret sbi_debug_console_read(unsigned long num_bytes,
                                      unsigned long base_addr_lo,
                                      unsigned long base_addr_hi) {
  return sbi_ecall(SBI_EID_CONSOLE, SBI_FID_CONSOLE_READ, num_bytes,
                   base_addr_lo, base_addr_hi, 0, 0, 0);
}

struct sbi_ret sbi_debug_console_write_byte(uint8_t byte) {
  return sbi_ecall(SBI_EID_CONSOLE, SBI_FID_CONSOLE_WRITE_BYTE, byte, 0, 0, 0,
                   0, 0);
}
