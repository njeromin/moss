#include "sbi.h"
#include "sbi/ecall.h"

#define SBI_EID_MPXY 0x4D505859

#define SBI_FID_MPXY_GET_SHMEM_SIZE 0
#define SBI_FID_MPXY_SET_SHMEM 1
#define SBI_FID_MPXY_GET_CHANNEL_IDS 2
#define SBI_FID_MPXY_READ_ATTRS 3
#define SBI_FID_MPXY_WRITE_ATTRS 4
#define SBI_FID_MPXY_SEND_MSG_WITH_RESP 5
#define SBI_FID_MPXY_SEND_MSG_WITHOUT_RESP 6
#define SBI_FID_MPXY_GET_NOTIFICATION_EVENTS 7

struct sbi_ret sbi_mpxy_get_shmem_size(void) {
  return sbi_ecall(SBI_EID_MPXY, SBI_FID_MPXY_GET_SHMEM_SIZE, 0, 0, 0, 0, 0, 0);
}

struct sbi_ret sbi_mpxy_set_shmem(unsigned long shmem_phys_lo,
                                  unsigned long shmem_phys_hi,
                                  unsigned long flags) {
  return sbi_ecall(SBI_EID_MPXY, SBI_FID_MPXY_SET_SHMEM, shmem_phys_lo,
                   shmem_phys_hi, flags, 0, 0, 0);
}

struct sbi_ret sbi_mpxy_get_channel_ids(uint32_t start_index) {
  return sbi_ecall(SBI_EID_MPXY, SBI_FID_MPXY_GET_CHANNEL_IDS, start_index, 0,
                   0, 0, 0, 0);
}

struct sbi_ret sbi_mpxy_read_attrs(uint32_t channel_id, uint32_t base_attr_id,
                                   uint32_t attr_count) {
  return sbi_ecall(SBI_EID_MPXY, SBI_FID_MPXY_READ_ATTRS, channel_id,
                   base_attr_id, attr_count, 0, 0, 0);
}

struct sbi_ret sbi_mpxy_write_attrs(uint32_t channel_id, uint32_t base_attr_id,
                                    uint32_t attr_count) {
  return sbi_ecall(SBI_EID_MPXY, SBI_FID_MPXY_WRITE_ATTRS, channel_id,
                   base_attr_id, attr_count, 0, 0, 0);
}

struct sbi_ret sbi_mpxy_send_msg_with_resp(uint32_t channel_id, uint32_t msg_id,
                                           unsigned long msg_len) {
  return sbi_ecall(SBI_EID_MPXY, SBI_FID_MPXY_SEND_MSG_WITH_RESP, channel_id,
                   msg_id, msg_len, 0, 0, 0);
}

struct sbi_ret sbi_mpxy_send_msg_without_resp(uint32_t channel_id,
                                              uint32_t msg_id,
                                              unsigned long msg_len) {
  return sbi_ecall(SBI_EID_MPXY, SBI_FID_MPXY_SEND_MSG_WITHOUT_RESP,
                   channel_id, msg_id, msg_len, 0, 0, 0);
}

struct sbi_ret sbi_mpxy_get_notification_events(uint32_t channel_id) {
  return sbi_ecall(SBI_EID_MPXY, SBI_FID_MPXY_GET_NOTIFICATION_EVENTS,
                   channel_id, 0, 0, 0, 0, 0);
}
