#include "sbi.h"
#include "sbi/ecall.h"

#define SBI_EID_BASE 0x10

#define SBI_FID_GET_SPEC_VERSION 0
#define SBI_FID_GET_IMPL_ID 1
#define SBI_FID_GET_IMPL_VERSION 2
#define SBI_FID_PROBE_EXTENSION 3
#define SBI_FID_GET_MVENDORID 4
#define SBI_FID_GET_MARCHID 5
#define SBI_FID_GET_MIMPID 6

struct sbi_ret sbi_get_spec_version(void) {
  return sbi_ecall(SBI_EID_BASE, SBI_FID_GET_SPEC_VERSION, 0, 0, 0, 0, 0, 0);
}

struct sbi_ret sbi_get_impl_id(void) {
  return sbi_ecall(SBI_EID_BASE, SBI_FID_GET_IMPL_ID, 0, 0, 0, 0, 0, 0);
}

struct sbi_ret sbi_get_impl_version(void) {
  return sbi_ecall(SBI_EID_BASE, SBI_FID_GET_IMPL_VERSION, 0, 0, 0, 0, 0, 0);
}

struct sbi_ret sbi_probe_extension(long extension_id) {
  return sbi_ecall(SBI_EID_BASE, SBI_FID_PROBE_EXTENSION, extension_id, 0, 0, 0,
                   0, 0);
}

struct sbi_ret sbi_get_mvendorid(void) {
  return sbi_ecall(SBI_EID_BASE, SBI_FID_GET_MVENDORID, 0, 0, 0, 0, 0, 0);
}

struct sbi_ret sbi_get_marchid(void) {
  return sbi_ecall(SBI_EID_BASE, SBI_FID_GET_MARCHID, 0, 0, 0, 0, 0, 0);
}

struct sbi_ret sbi_get_mimpid(void) {
  return sbi_ecall(SBI_EID_BASE, SBI_FID_GET_MIMPID, 0, 0, 0, 0, 0, 0);
}
