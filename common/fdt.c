#include <common/fdt.h>
#include <common/endian.h>

#define FDT_MAGIC 0xd00dfeedU
#define FDT_MIN_VERSION 16U
#define FDT_MAX_VERSION 17U
#define FDT_HEADER_SIZE 40U
#define FDT_RESERVE_ENTRY_SIZE 16U

static int fdt_range_valid(uint32_t offset, uint32_t size, uint32_t total) {
  return offset <= total && size <= total - offset;
}

static size_t fdt_string_length(const uint8_t* strings, size_t size,
                                uint32_t offset) {
  size_t i;

  if (offset >= size) {
    return 0;
  }

  for (i = offset; i < size; ++i) {
    if (strings[i] == 0) {
      return i - offset + 1;
    }
  }

  return 0;
}

int fdt_validate(const void* dtb_blob) {
  const uint8_t* blob = (const uint8_t*)dtb_blob;
  uint32_t total, struct_offset, strings_offset, reserve_offset;
  uint32_t struct_size, strings_size, version, last_version;
  const uint8_t *structure, *strings, *reserve;
  size_t pos, depth = 0;
  int saw_end = 0;

  if (blob == 0 || be32toh(blob) != FDT_MAGIC) {
    return -1;
  }

  total = be32toh(blob + 4);
  struct_offset = be32toh(blob + 8);
  strings_offset = be32toh(blob + 12);
  reserve_offset = be32toh(blob + 16);
  version = be32toh(blob + 20);
  last_version = be32toh(blob + 24);
  strings_size = be32toh(blob + 32);
  struct_size = be32toh(blob + 36);

  if (total < FDT_HEADER_SIZE || version < FDT_MIN_VERSION ||
      version > FDT_MAX_VERSION || last_version > version ||
      last_version < FDT_MIN_VERSION || (struct_offset & 3U) != 0 ||
      (strings_offset & 3U) != 0 || (reserve_offset & 7U) != 0 ||
      !fdt_range_valid(struct_offset, struct_size, total) ||
      !fdt_range_valid(strings_offset, strings_size, total) ||
      reserve_offset > total - FDT_RESERVE_ENTRY_SIZE) {
    return -1;
  }

  structure = blob + struct_offset;
  strings = blob + strings_offset;
  reserve = blob + reserve_offset;

  for (pos = 0; pos + FDT_RESERVE_ENTRY_SIZE <= total - reserve_offset;
       pos += FDT_RESERVE_ENTRY_SIZE) {
    if (be64toh(reserve + pos) == 0 && be64toh(reserve + pos + 8) == 0) {
      break;
    }
  }
  if (reserve_offset + pos + FDT_RESERVE_ENTRY_SIZE > total) {
    return -1;
  }

  for (pos = 0; pos + 4 <= struct_size;) {
    uint32_t token = be32toh(structure + pos);
    pos += 4;
    if (token == FDT_BEGIN_NODE) {
      size_t name_len = 0;
      while (pos + name_len < struct_size && structure[pos + name_len] != 0) {
        ++name_len;
      }
      if (pos + name_len >= struct_size) {
        return -1;
      }
      pos += (name_len + 1 + 3) & ~((size_t)3);
      if (pos > struct_size) {
        return -1;
      }
      ++depth;
    } else if (token == FDT_END_NODE) {
      if (depth == 0) {
        return -1;
      }
      --depth;
    } else if (token == FDT_PROP) {
      uint32_t len, nameoff;
      size_t padded;
      if (pos + 8 > struct_size) {
        return -1;
      }
      len = be32toh(structure + pos);
      nameoff = be32toh(structure + pos + 4);
      pos += 8;
      padded = ((size_t)len + 3) & ~((size_t)3);
      if (padded < len || pos > struct_size - padded ||
          fdt_string_length(strings, strings_size, nameoff) == 0) {
        return -1;
      }
      pos += padded;
    } else if (token == FDT_NOP) {
      continue;
    } else if (token == FDT_END) {
      if (depth != 0) {
        return -1;
      }
      saw_end = 1;
      break;
    } else {
      return -1;
    }
  }

  return saw_end ? 0 : -1;
}
