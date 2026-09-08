#include "sbi.h"
#include "sbi/ecall.h"

#define SBI_EID_TIMER 0x54494D45

#define SBI_FID_SET_TIMER 0

struct sbi_ret sbi_set_timer(uint64_t stime_value) {
  return sbi_ecall(SBI_EID_TIMER, SBI_FID_SET_TIMER, stime_value, 0, 0, 0, 0,
                   0);
}
