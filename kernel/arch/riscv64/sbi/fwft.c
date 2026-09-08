#include "sbi.h"
#include "sbi/ecall.h"

#define SBI_EID_FWFT 0x46574654

#define SBI_FID_FWFT_SET 0
#define SBI_FID_FWFT_GET 1

struct sbi_ret sbi_fwft_set(uint32_t feature, unsigned long value,
                            unsigned long flags) {
  return sbi_ecall(SBI_EID_FWFT, SBI_FID_FWFT_SET, feature, value, flags, 0, 0,
                   0);
}

struct sbi_ret sbi_fwft_get(uint32_t feature) {
  return sbi_ecall(SBI_EID_FWFT, SBI_FID_FWFT_GET, feature, 0, 0, 0, 0, 0);
}
