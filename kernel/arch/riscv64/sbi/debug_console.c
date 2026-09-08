#include "sbi.h"
#include "sbi/ecall.h"

#define SBI_EID_CONSOLE 0x4442434E

#define SBI_FID_CONSOLE_WRITE 0
#define SBI_FID_CONSOLE_READ 1
#define SBI_FID_CONSOLE_WRITE_BYTE 2

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
