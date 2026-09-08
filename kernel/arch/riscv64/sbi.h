#pragma once

#include <stdint.h>

enum sbi_error {
  SBI_SUCCESS = 0,
  SBI_ERR_FAILED = -1,
  SBI_ERR_NOT_SUPPORTED = -2,
  SBI_ERR_INVALID_PARAM = -3,
  SBI_ERR_DENIED = -4,
  SBI_ERR_INVALID_ADDRESS = -5,
  SBI_ERR_ALREADY_AVAILABLE = -6,
  SBI_ERR_ALREADY_STARTED = -7,
  SBI_ERR_ALREADY_STOPPED = -8,
  SBI_ERR_NO_SHMEM = -9,
  SBI_ERR_INVALID_STATE = -10,
  SBI_ERR_BAD_RANGE = -11,
  SBI_ERR_TIMEOUT = -12,
  SBI_ERR_IO = -13,
  SBI_ERR_DENIED_LOCKED = -14,
};

struct sbi_ret {
  long error;

  union {
    long value;
    unsigned long uvalue;
  };
};

/* Base Extension */

#define SBI_SPEC_VERSION_MAJOR_MASK 0x7F000000UL
#define SBI_SPEC_VERSION_MAJOR_SHIFT 24
#define SBI_SPEC_VERSION_MINOR_MASK 0x00FFFFFFUL
#define SBI_SPEC_VERSION_MINOR_SHIFT 0

#define SBI_SPEC_VERSION_MAJOR(v) (((v) >> SBI_SPEC_VERSION_MAJOR_SHIFT) & 0x7FU)
#define SBI_SPEC_VERSION_MINOR(v) ((v) & SBI_SPEC_VERSION_MINOR_MASK)

/* Standard Implementation IDs */
#define SBI_IMPL_ID_BBL 0
#define SBI_IMPL_ID_OPENSBI 1
#define SBI_IMPL_ID_XVISOR 2
#define SBI_IMPL_ID_KVM 3
#define SBI_IMPL_ID_RUSTSBI 4
#define SBI_IMPL_ID_DRONE 5
#define SBI_IMPL_ID_CHARIOT 6
#define SBI_IMPL_ID_ANDES_CODENSE 7

/* Standard Extension IDs (EIDs) */
#define SBI_EXT_BASE 0x10
#define SBI_EXT_TIMER 0x54494D45
#define SBI_EXT_IPI 0x735049
#define SBI_EXT_RFENCE 0x52464E43
#define SBI_EXT_HSM 0x48534D
#define SBI_EXT_SRST 0x53525354
#define SBI_EXT_PMU 0x504D55
#define SBI_EXT_DBCN 0x4442434E
#define SBI_EXT_MPXY 0x4D505859
#define SBI_EXT_FWFT 0x46574654

struct sbi_ret sbi_get_spec_version(void);
struct sbi_ret sbi_get_impl_id(void);
struct sbi_ret sbi_get_impl_version(void);
struct sbi_ret sbi_probe_extension(long extension_id);
struct sbi_ret sbi_get_mvendorid(void);
struct sbi_ret sbi_get_marchid(void);
struct sbi_ret sbi_get_mimpid(void);


/* Timer Extension */

struct sbi_ret sbi_set_timer(uint64_t stime_value);

/* IPI Extension */

struct sbi_ret sbi_send_ipi(unsigned long hart_mask,
                            unsigned long hart_mask_base);

/* Console Extension */

struct sbi_ret sbi_debug_console_write(unsigned long num_bytes,
                                       unsigned long base_addr_lo,
                                       unsigned long base_addr_hi);
struct sbi_ret sbi_debug_console_read(unsigned long num_bytes,
                                      unsigned long base_addr_lo,
                                      unsigned long base_addr_hi);
struct sbi_ret sbi_debug_console_write_byte(uint8_t byte);

/* Hart State Management Extension */

enum sbi_hart_state {
  SBI_HART_STARTED = 0,
  SBI_HART_STOPPED = 1,
  SBI_HART_START_PENDING = 2,
  SBI_HART_STOP_PENDING = 3,
  SBI_HART_SUSPENDED = 4,
  SBI_HART_SUSPEND_PENDING = 5,
  SBI_HART_RESUME_PENDING = 6,
};

#define SBI_HART_SUSPEND_RETENTIVE 0x00000000U
#define SBI_HART_SUSPEND_NON_RETENTIVE 0x80000000U

struct sbi_ret sbi_hart_start(unsigned long hartid, unsigned long start_addr,
                              unsigned long opaque);
struct sbi_ret sbi_hart_stop(void);
struct sbi_ret sbi_hart_get_status(unsigned long hartid);
struct sbi_ret sbi_hart_suspend(uint32_t suspend_type,
                                unsigned long resume_addr,
                                unsigned long opaque);

/* Remote Fence Extension */

struct sbi_ret sbi_remote_fence_i(unsigned long hart_mask,
                                  unsigned long hart_mask_base);
struct sbi_ret sbi_remote_sfence_vma(unsigned long hart_mask,
                                     unsigned long hart_mask_base,
                                     unsigned long start_addr,
                                     unsigned long size);
struct sbi_ret sbi_remote_sfence_vma_asid(unsigned long hart_mask,
                                          unsigned long hart_mask_base,
                                          unsigned long start_addr,
                                          unsigned long size,
                                          unsigned long asid);
struct sbi_ret sbi_remote_hfence_gvma_vmid(unsigned long hart_mask,
                                           unsigned long hart_mask_base,
                                           unsigned long start_addr,
                                           unsigned long size,
                                           unsigned long vmid);
struct sbi_ret sbi_remote_hfence_gvma(unsigned long hart_mask,
                                      unsigned long hart_mask_base,
                                      unsigned long start_addr,
                                      unsigned long size);
struct sbi_ret sbi_remote_hfence_vvma_asid(unsigned long hart_mask,
                                           unsigned long hart_mask_base,
                                           unsigned long start_addr,
                                           unsigned long size,
                                           unsigned long asid);
struct sbi_ret sbi_remote_hfence_vvma(unsigned long hart_mask,
                                      unsigned long hart_mask_base,
                                      unsigned long start_addr,
                                      unsigned long size);

/* Performance Monitoring Unit Extension */

/* Hardware general events */
#define SBI_PMU_HW_NO_EVENT 0
#define SBI_PMU_HW_CPU_CYCLES 1
#define SBI_PMU_HW_INSTRUCTIONS 2
#define SBI_PMU_HW_CACHE_REFERENCES 3
#define SBI_PMU_HW_CACHE_MISSES 4
#define SBI_PMU_HW_BRANCH_INSTRUCTIONS 5
#define SBI_PMU_HW_BRANCH_MISSES 6
#define SBI_PMU_HW_BUS_CYCLES 7
#define SBI_PMU_HW_STALLED_CYCLES_FRONTEND 8
#define SBI_PMU_HW_STALLED_CYCLES_BACKEND 9
#define SBI_PMU_HW_REF_CPU_CYCLES 10

/* Counter start flags */
#define SBI_PMU_START_SET_INIT_VALUE (1UL << 0)
#define SBI_PMU_START_FLAG_INIT_SNAPSHOT (1UL << 1)

/* Counter stop flags */
#define SBI_PMU_STOP_FLAG_RESET (1UL << 0)
#define SBI_PMU_STOP_FLAG_TAKE_SNAPSHOT (1UL << 1)

/* Counter config flags */
#define SBI_PMU_CFG_FLAG_INHERIT (1UL << 0)
#define SBI_PMU_CFG_FLAG_MATCH_UVR (1UL << 1)
#define SBI_PMU_CFG_FLAG_CLEAR_OVERFLOW (1UL << 2)
#define SBI_PMU_CFG_FLAG_AUTO_START (1UL << 3)
#define SBI_PMU_CFG_FLAG_SET_VUINH (1UL << 4)
#define SBI_PMU_CFG_FLAG_SET_VSINH (1UL << 5)
#define SBI_PMU_CFG_FLAG_SET_UINH (1UL << 6)
#define SBI_PMU_CFG_FLAG_SET_SINH (1UL << 7)
#define SBI_PMU_CFG_FLAG_SET_MINH (1UL << 8)

struct sbi_ret sbi_pmu_num_counters(void);
struct sbi_ret sbi_pmu_counter_get_info(unsigned long counter_idx);
struct sbi_ret sbi_pmu_counter_config_matching(unsigned long counter_idx_base,
                                               unsigned long counter_idx_mask,
                                               unsigned long config_flags,
                                               unsigned long event_idx,
                                               uint64_t event_data);
struct sbi_ret sbi_pmu_counter_start(unsigned long counter_idx_base,
                                     unsigned long counter_idx_mask,
                                     unsigned long start_flags,
                                     uint64_t initial_value);
struct sbi_ret sbi_pmu_counter_stop(unsigned long counter_idx_base,
                                    unsigned long counter_idx_mask,
                                    unsigned long stop_flags);
struct sbi_ret sbi_pmu_counter_fw_read(unsigned long counter_idx);
struct sbi_ret sbi_pmu_counter_fw_read_hi(unsigned long counter_idx);
struct sbi_ret sbi_pmu_snapshot_set_shmem(unsigned long shmem_phys_lo,
                                          unsigned long shmem_phys_hi,
                                          unsigned long flags);
struct sbi_ret sbi_pmu_event_get_info(unsigned long shmem_phys_lo,
                                      unsigned long shmem_phys_hi,
                                      unsigned long num_entries,
                                      unsigned long flags);

/* System Reset Extension */

#define SBI_SRST_RESET_TYPE_SHUTDOWN 0x00000000U
#define SBI_SRST_RESET_TYPE_COLD_REBOOT 0x00000001U
#define SBI_SRST_RESET_TYPE_WARM_REBOOT 0x00000002U

#define SBI_SRST_RESET_REASON_NONE 0x00000000U
#define SBI_SRST_RESET_REASON_SYSFAIL 0x00000001U

struct sbi_ret sbi_system_reset(uint32_t reset_type, uint32_t reset_reason);

/* Message Proxy (MPXY) Extension */

#define SBI_MPXY_ATTR_MSG_PROT_ID 0x00000000U
#define SBI_MPXY_ATTR_MSG_PROT_VER 0x00000001U
#define SBI_MPXY_ATTR_MSG_MAX_LEN 0x00000002U
#define SBI_MPXY_ATTR_MSG_SEND_TIMEOUT 0x00000003U
#define SBI_MPXY_ATTR_MSG_COMPLETION_TIMEOUT 0x00000004U

#define SBI_MPXY_MSG_PROT_RPMI 0x00000000U
#define SBI_MPXY_MSG_PROT_MBOX 0x00000001U

struct sbi_ret sbi_mpxy_get_shmem_size(void);
struct sbi_ret sbi_mpxy_set_shmem(unsigned long shmem_phys_lo,
                                  unsigned long shmem_phys_hi,
                                  unsigned long flags);
struct sbi_ret sbi_mpxy_get_channel_ids(uint32_t start_index);
struct sbi_ret sbi_mpxy_read_attrs(uint32_t channel_id, uint32_t base_attr_id,
                                   uint32_t attr_count);
struct sbi_ret sbi_mpxy_write_attrs(uint32_t channel_id, uint32_t base_attr_id,
                                    uint32_t attr_count);
struct sbi_ret sbi_mpxy_send_msg_with_resp(uint32_t channel_id, uint32_t msg_id,
                                           unsigned long msg_len);
struct sbi_ret sbi_mpxy_send_msg_without_resp(uint32_t channel_id,
                                              uint32_t msg_id,
                                              unsigned long msg_len);
struct sbi_ret sbi_mpxy_get_notification_events(uint32_t channel_id);

/* Firmware Features (FWFT) Extension */

#define SBI_FWFT_FLAG_LOCK (1UL << 0)

#define SBI_FWFT_MISALIGNED_EXC_DELEG 0x00000000U
#define SBI_FWFT_LANDING_PAD 0x00000001U
#define SBI_FWFT_SHADOW_STACK 0x00000002U
#define SBI_FWFT_DOUBLE_TRAP 0x00000003U
#define SBI_FWFT_PTE_AD_HW_UPDATING 0x00000004U
#define SBI_FWFT_POINTER_MASKING_PMLEN 0x00000005U

struct sbi_ret sbi_fwft_set(uint32_t feature, unsigned long value,
                            unsigned long flags);
struct sbi_ret sbi_fwft_get(uint32_t feature);
