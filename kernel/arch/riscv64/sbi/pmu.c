#include "sbi.h"
#include "sbi/ecall.h"

#define SBI_EID_PMU 0x504D55

#define SBI_FID_PMU_NUM_COUNTERS 0
#define SBI_FID_PMU_COUNTER_GET_INFO 1
#define SBI_FID_PMU_COUNTER_CONFIG_MATCHING 2
#define SBI_FID_PMU_COUNTER_START 3
#define SBI_FID_PMU_COUNTER_STOP 4
#define SBI_FID_PMU_COUNTER_FW_READ 5
#define SBI_FID_PMU_COUNTER_FW_READ_HI 6
#define SBI_FID_PMU_SNAPSHOT_SET_SHMEM 7
#define SBI_FID_PMU_EVENT_GET_INFO 8

struct sbi_ret sbi_pmu_num_counters(void) {
  return sbi_ecall(SBI_EID_PMU, SBI_FID_PMU_NUM_COUNTERS, 0, 0, 0, 0, 0, 0);
}

struct sbi_ret sbi_pmu_counter_get_info(unsigned long counter_idx) {
  return sbi_ecall(SBI_EID_PMU, SBI_FID_PMU_COUNTER_GET_INFO, counter_idx, 0, 0,
                   0, 0, 0);
}

struct sbi_ret sbi_pmu_counter_config_matching(unsigned long counter_idx_base,
                                               unsigned long counter_idx_mask,
                                               unsigned long config_flags,
                                               unsigned long event_idx,
                                               uint64_t event_data) {
  return sbi_ecall(SBI_EID_PMU, SBI_FID_PMU_COUNTER_CONFIG_MATCHING,
                   counter_idx_base, counter_idx_mask, config_flags, event_idx,
                   event_data, 0);
}

struct sbi_ret sbi_pmu_counter_start(unsigned long counter_idx_base,
                                     unsigned long counter_idx_mask,
                                     unsigned long start_flags,
                                     uint64_t initial_value) {
  return sbi_ecall(SBI_EID_PMU, SBI_FID_PMU_COUNTER_START, counter_idx_base,
                   counter_idx_mask, start_flags, initial_value, 0, 0);
}

struct sbi_ret sbi_pmu_counter_stop(unsigned long counter_idx_base,
                                    unsigned long counter_idx_mask,
                                    unsigned long stop_flags) {
  return sbi_ecall(SBI_EID_PMU, SBI_FID_PMU_COUNTER_STOP, counter_idx_base,
                   counter_idx_mask, stop_flags, 0, 0, 0);
}

struct sbi_ret sbi_pmu_counter_fw_read(unsigned long counter_idx) {
  return sbi_ecall(SBI_EID_PMU, SBI_FID_PMU_COUNTER_FW_READ, counter_idx, 0, 0,
                   0, 0, 0);
}

struct sbi_ret sbi_pmu_counter_fw_read_hi(unsigned long counter_idx) {
  return sbi_ecall(SBI_EID_PMU, SBI_FID_PMU_COUNTER_FW_READ_HI, counter_idx, 0,
                   0, 0, 0, 0);
}

struct sbi_ret sbi_pmu_snapshot_set_shmem(unsigned long shmem_phys_lo,
                                          unsigned long shmem_phys_hi,
                                          unsigned long flags) {
  return sbi_ecall(SBI_EID_PMU, SBI_FID_PMU_SNAPSHOT_SET_SHMEM, shmem_phys_lo,
                   shmem_phys_hi, flags, 0, 0, 0);
}

struct sbi_ret sbi_pmu_event_get_info(unsigned long shmem_phys_lo,
                                      unsigned long shmem_phys_hi,
                                      unsigned long num_entries,
                                      unsigned long flags) {
  return sbi_ecall(SBI_EID_PMU, SBI_FID_PMU_EVENT_GET_INFO, shmem_phys_lo,
                   shmem_phys_hi, num_entries, flags, 0, 0);
}
