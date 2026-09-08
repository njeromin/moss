#include "sbi.h"
#include "sbi/ecall.h"

#define SBI_EID_SRST 0x53525354

#define SBI_FID_SYSTEM_RESET 0

struct sbi_ret sbi_system_reset(uint32_t reset_type, uint32_t reset_reason) {
  return sbi_ecall(SBI_EID_SRST, SBI_FID_SYSTEM_RESET, reset_type, reset_reason,
                   0, 0, 0, 0);
}
