#include "sbi.h"
#include "sbi/ecall.h"

#define SBI_EID_HSM 0x48534D

#define SBI_FID_HART_START 0
#define SBI_FID_HART_STOP 1
#define SBI_FID_HART_GET_STATUS 2
#define SBI_FID_HART_SUSPEND 3

struct sbi_ret sbi_hart_start(unsigned long hartid, unsigned long start_addr,
                              unsigned long opaque) {
  return sbi_ecall(SBI_EID_HSM, SBI_FID_HART_START, hartid, start_addr, opaque,
                   0, 0, 0);
}

struct sbi_ret sbi_hart_stop(void) {
  return sbi_ecall(SBI_EID_HSM, SBI_FID_HART_STOP, 0, 0, 0, 0, 0, 0);
}

struct sbi_ret sbi_hart_get_status(unsigned long hartid) {
  return sbi_ecall(SBI_EID_HSM, SBI_FID_HART_GET_STATUS, hartid, 0, 0, 0, 0, 0);
}

struct sbi_ret sbi_hart_suspend(uint32_t suspend_type,
                                unsigned long resume_addr,
                                unsigned long opaque) {
  return sbi_ecall(SBI_EID_HSM, SBI_FID_HART_SUSPEND, suspend_type,
                   resume_addr, opaque, 0, 0, 0);
}
