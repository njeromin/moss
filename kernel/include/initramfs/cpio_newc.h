#pragma once

#include <stdint.h>

#define CPIO_NEWC_MAGIC "070701"
#define CPIO_NEWC_MAGIC_SIZE 6
#define CPIO_NEWC_TRAILER "TRAILER!!!"

struct cpio_newc_header {
  char c_magic[6];
  char c_ino[8];
  char c_mode[8];
  char c_uid[8];
  char c_gid[8];
  char c_nlink[8];
  char c_mtime[8];
  char c_filesize[8];
  char c_maj[8];
  char c_min[8];
  char c_rmaj[8];
  char c_rmin[8];
  char c_namesize[8];
  char c_check[8];
};
