#pragma once

#include <stdint.h>

struct sbi_ret_t {
  long error;
  long value;
};
/* Base Extension */

struct sbi_ret_t sbi_get_spec_version(void);

/* Timer Extension */

struct sbi_ret_t sbi_set_timer(uint64_t stime_value);

/* Console Extension */

struct sbi_ret_t sbi_debug_console_write(unsigned long num_bytes,
                                         unsigned long base_addr_lo,
                                         unsigned long base_addr_hi);
