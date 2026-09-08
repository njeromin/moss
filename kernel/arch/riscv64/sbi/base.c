#include "sbi.h"
#include "sbi/ecall.h"

#define SBI_EID_BASE 0x10

#define SBI_FID_GET_SPEC_VERSION 0

struct sbi_ret sbi_get_spec_version(void) {
  return sbi_ecall(SBI_EID_BASE, SBI_FID_GET_SPEC_VERSION, 0, 0, 0, 0, 0, 0);
}
