#include <common/endian.h>

uint32_t be32toh(const uint8_t bytes[4]) {
  return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
         ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

uint64_t be64toh(const uint8_t bytes[8]) {
  return ((uint64_t)be32toh(bytes) << 32) | be32toh(bytes + 4);
}
