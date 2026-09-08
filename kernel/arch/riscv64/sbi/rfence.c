#include "sbi.h"
#include "sbi/ecall.h"

#define SBI_EID_RFENCE 0x52464E43

#define SBI_FID_REMOTE_FENCE_I 0
#define SBI_FID_REMOTE_SFENCE_VMA 1
#define SBI_FID_REMOTE_SFENCE_VMA_ASID 2
#define SBI_FID_REMOTE_HFENCE_GVMA_VMID 3
#define SBI_FID_REMOTE_HFENCE_GVMA 4
#define SBI_FID_REMOTE_HFENCE_VVMA_ASID 5
#define SBI_FID_REMOTE_HFENCE_VVMA 6

struct sbi_ret sbi_remote_fence_i(unsigned long hart_mask,
                                  unsigned long hart_mask_base) {
  return sbi_ecall(SBI_EID_RFENCE, SBI_FID_REMOTE_FENCE_I, hart_mask,
                   hart_mask_base, 0, 0, 0, 0);
}

struct sbi_ret sbi_remote_sfence_vma(unsigned long hart_mask,
                                     unsigned long hart_mask_base,
                                     unsigned long start_addr,
                                     unsigned long size) {
  return sbi_ecall(SBI_EID_RFENCE, SBI_FID_REMOTE_SFENCE_VMA, hart_mask,
                   hart_mask_base, start_addr, size, 0, 0);
}

struct sbi_ret sbi_remote_sfence_vma_asid(unsigned long hart_mask,
                                          unsigned long hart_mask_base,
                                          unsigned long start_addr,
                                          unsigned long size,
                                          unsigned long asid) {
  return sbi_ecall(SBI_EID_RFENCE, SBI_FID_REMOTE_SFENCE_VMA_ASID, hart_mask,
                   hart_mask_base, start_addr, size, asid, 0);
}

struct sbi_ret sbi_remote_hfence_gvma_vmid(unsigned long hart_mask,
                                           unsigned long hart_mask_base,
                                           unsigned long start_addr,
                                           unsigned long size,
                                           unsigned long vmid) {
  return sbi_ecall(SBI_EID_RFENCE, SBI_FID_REMOTE_HFENCE_GVMA_VMID, hart_mask,
                   hart_mask_base, start_addr, size, vmid, 0);
}

struct sbi_ret sbi_remote_hfence_gvma(unsigned long hart_mask,
                                      unsigned long hart_mask_base,
                                      unsigned long start_addr,
                                      unsigned long size) {
  return sbi_ecall(SBI_EID_RFENCE, SBI_FID_REMOTE_HFENCE_GVMA, hart_mask,
                   hart_mask_base, start_addr, size, 0, 0);
}

struct sbi_ret sbi_remote_hfence_vvma_asid(unsigned long hart_mask,
                                           unsigned long hart_mask_base,
                                           unsigned long start_addr,
                                           unsigned long size,
                                           unsigned long asid) {
  return sbi_ecall(SBI_EID_RFENCE, SBI_FID_REMOTE_HFENCE_VVMA_ASID, hart_mask,
                   hart_mask_base, start_addr, size, asid, 0);
}

struct sbi_ret sbi_remote_hfence_vvma(unsigned long hart_mask,
                                      unsigned long hart_mask_base,
                                      unsigned long start_addr,
                                      unsigned long size) {
  return sbi_ecall(SBI_EID_RFENCE, SBI_FID_REMOTE_HFENCE_VVMA, hart_mask,
                   hart_mask_base, start_addr, size, 0, 0);
}
