#include "sbi.h"
#include "sbi/ecall.h"

#define SBI_EID_IPI 0x735049

#define SBI_FID_SEND_IPI 0

struct sbi_ret sbi_send_ipi(unsigned long hart_mask,
                            unsigned long hart_mask_base) {
  return sbi_ecall(SBI_EID_IPI, SBI_FID_SEND_IPI, hart_mask, hart_mask_base, 0,
                   0, 0, 0);
}
