#include <common/endian.h>
#include <common/fdt.h>
#include <common/strings.h>

#define FDT_MAGIC 0xd00dfeedU
#define FDT_MIN_VERSION 16U
#define FDT_MAX_VERSION 17U
#define FDT_HEADER_SIZE 40U
#define FDT_RESERVE_ENTRY_SIZE 16U

struct fdt_view {
  const uint8_t* blob;
  const uint8_t* structure;
  const uint8_t* strings;
  uint32_t struct_size;
  uint32_t strings_size;
};

static int fdt_range_valid(uint32_t offset, uint32_t size, uint32_t total) {
  return offset <= total && size <= total - offset;
}

static int fdt_view_init(const void* blob_ptr, struct fdt_view* view) {
  const uint8_t* blob = (const uint8_t*)blob_ptr;
  uint32_t total;
  if (view == 0 || fdt_check_header(blob) != 0) {
    return -1;
  }
  total = be32toh(blob + 4);
  view->blob = blob;
  view->structure = blob + be32toh(blob + 8);
  view->strings = blob + be32toh(blob + 12);
  view->struct_size = be32toh(blob + 36);
  view->strings_size = be32toh(blob + 32);
  return total >= view->struct_size ? 0 : -1;
}

static int fdt_next_token(const struct fdt_view* view, uint32_t offset,
                          uint32_t* token, uint32_t* next_offset) {
  size_t name_size, padded;
  uint32_t len;
  if (view == 0 || token == 0 || next_offset == 0 || view->struct_size < 4 ||
      offset > view->struct_size - 4) {
    return -1;
  }
  *token = be32toh(view->structure + offset);
  if (*token == FDT_BEGIN_NODE) {
    if (string_length_bounded((const char*)(view->structure + offset + 4),
                              view->struct_size - offset - 4,
                              &name_size) != 0) {
      return -1;
    }
    padded = (name_size + 3) & ~((size_t)3);
    if (padded > view->struct_size - offset - 4) {
      return -1;
    }
    *next_offset = offset + 4 + (uint32_t)padded;
  } else if (*token == FDT_PROP) {
    if (offset > view->struct_size - 12) {
      return -1;
    }
    len = be32toh(view->structure + offset + 4);
    padded = ((size_t)len + 3) & ~((size_t)3);
    if (padded < len || padded > view->struct_size - offset - 12) {
      return -1;
    }
    *next_offset = offset + 12 + (uint32_t)padded;
  } else if (*token == FDT_NOP || *token == FDT_END_NODE || *token == FDT_END) {
    *next_offset = offset + 4;
  } else {
    return -1;
  }
  return 0;
}

static int fdt_node_begin(const struct fdt_view* view,
                          const struct fdt_node* node, uint32_t* next) {
  uint32_t token;
  if (node == 0 || fdt_next_token(view, node->offset, &token, next) != 0 ||
      token != FDT_BEGIN_NODE) {
    return -1;
  }
  return 0;
}

static int fdt_names_equal(const char* left, const uint8_t* right,
                           size_t right_size) {
  size_t i, left_size, right_len;
  if (left == 0 ||
      string_length_bounded((const char*)right, right_size, &right_len) != 0) {
    return 0;
  }
  left_size = string_length(left) + 1;
  if (left_size != right_len) {
    return 0;
  }
  for (i = 0; i < left_size; ++i) {
    if ((uint8_t)left[i] != right[i]) {
      return 0;
    }
  }
  return 1;
}

int fdt_check_header(const void* dtb_blob) {
  const uint8_t* blob = (const uint8_t*)dtb_blob;
  uint32_t total, struct_offset, strings_offset, reserve_offset;
  uint32_t struct_size, strings_size, version, last_version;

  if (blob == 0 || ((uintptr_t)blob & 7U) != 0 || be32toh(blob) != FDT_MAGIC) {
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

  return 0;
}

int fdt_check_full(const void* dtb_blob, size_t blob_size) {
  const uint8_t* blob = (const uint8_t*)dtb_blob;
  const uint8_t *structure, *strings, *reserve;
  uint32_t total, struct_offset, strings_offset, reserve_offset;
  uint32_t struct_size, strings_size;
  size_t pos, depth = 0;
  int saw_end = 0;

  if (blob == 0 || blob_size < FDT_HEADER_SIZE || fdt_check_header(blob) != 0) {
    return -1;
  }

  total = be32toh(blob + 4);
  if ((size_t)total > blob_size) {
    return -1;
  }
  struct_offset = be32toh(blob + 8);
  strings_offset = be32toh(blob + 12);
  reserve_offset = be32toh(blob + 16);
  strings_size = be32toh(blob + 32);
  struct_size = be32toh(blob + 36);

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
      size_t name_size;
      if (string_length_bounded((const char*)(structure + pos),
                                struct_size - pos, &name_size) != 0) {
        return -1;
      }
      pos += (name_size + 3) & ~((size_t)3);
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
      size_t padded, name_size;
      if (pos + 8 > struct_size) {
        return -1;
      }
      len = be32toh(structure + pos);
      nameoff = be32toh(structure + pos + 4);
      pos += 8;
      padded = ((size_t)len + 3) & ~((size_t)3);
      if (padded < len || pos > struct_size - padded ||
          nameoff >= strings_size ||
          string_length_bounded((const char*)(strings + nameoff),
                                strings_size - nameoff, &name_size) != 0) {
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

int fdt_root(const void* blob, struct fdt_node* node) {
  struct fdt_view view;
  uint32_t token, next;
  if (node == 0 || fdt_view_init(blob, &view) != 0 ||
      fdt_next_token(&view, 0, &token, &next) != 0 || token != FDT_BEGIN_NODE) {
    return -1;
  }
  node->offset = 0;
  return 0;
}

int fdt_first_child(const void* blob, const struct fdt_node* parent,
                    struct fdt_node* child) {
  struct fdt_view view;
  uint32_t pos, token, next;
  if (child == 0 || fdt_view_init(blob, &view) != 0 ||
      fdt_node_begin(&view, parent, &pos) != 0) {
    return -1;
  }
  while (pos < view.struct_size) {
    if (fdt_next_token(&view, pos, &token, &next) != 0) {
      return -1;
    }
    if (token == FDT_BEGIN_NODE) {
      child->offset = pos;
      return 0;
    }
    if (token == FDT_END_NODE) {
      return -1;
    }
    pos = next;
  }
  return -1;
}

int fdt_next_sibling(const void* blob, const struct fdt_node* node,
                     struct fdt_node* sibling) {
  struct fdt_view view;
  uint32_t pos, token, next;
  unsigned depth = 0;
  if (sibling == 0 || fdt_view_init(blob, &view) != 0 ||
      fdt_node_begin(&view, node, &pos) != 0) {
    return -1;
  }
  while (pos < view.struct_size) {
    if (fdt_next_token(&view, pos, &token, &next) != 0) {
      return -1;
    }
    if (token == FDT_BEGIN_NODE) {
      ++depth;
    } else if (token == FDT_END_NODE) {
      if (depth == 0) {
        pos = next;
        if (pos >= view.struct_size ||
            fdt_next_token(&view, pos, &token, &next) != 0 ||
            token != FDT_BEGIN_NODE) {
          return -1;
        }
        sibling->offset = pos;
        return 0;
      }
      --depth;
    } else if (token == FDT_END) {
      return -1;
    }
    pos = next;
  }
  return -1;
}

int fdt_node_name(const void* blob, const struct fdt_node* node,
                  const char** name, size_t* size) {
  struct fdt_view view;
  uint32_t next, name_offset;
  size_t length;
  if (name == 0 || size == 0 || fdt_view_init(blob, &view) != 0 ||
      fdt_node_begin(&view, node, &next) != 0) {
    return -1;
  }
  if (node->offset > view.struct_size - 4) return -1;
  name_offset = node->offset + 4;
  if (name_offset > view.struct_size ||
      string_length_bounded((const char*)(view.structure + name_offset),
                            view.struct_size - name_offset, &length) != 0) {
    return -1;
  }
  *name = (const char*)(view.structure + name_offset);
  *size = length - 1;
  return 0;
}

int fdt_node_name_is(const void* blob, const struct fdt_node* node,
                     const char* name) {
  const char* node_name;
  size_t node_name_size, name_size, i;
  if (name == 0 ||
      fdt_node_name(blob, node, &node_name, &node_name_size) != 0) {
    return 0;
  }
  name_size = string_length(name);
  if (node_name_size != name_size) {
    return 0;
  }
  for (i = 0; i < name_size; ++i) {
    if (node_name[i] != name[i]) {
      return 0;
    }
  }
  return 1;
}

int fdt_get_property(const void* blob, const struct fdt_node* node,
                     const char* name, const void** data, size_t* size) {
  struct fdt_view view;
  uint32_t pos, token, next;
  if (data == 0 || size == 0 || name == 0 || fdt_view_init(blob, &view) != 0 ||
      fdt_node_begin(&view, node, &pos) != 0) {
    return -1;
  }
  while (pos < view.struct_size) {
    if (fdt_next_token(&view, pos, &token, &next) != 0) {
      return -1;
    }
    if (token == FDT_END_NODE || token == FDT_BEGIN_NODE) {
      return -1;
    }
    if (token == FDT_PROP) {
      uint32_t len = be32toh(view.structure + pos + 4);
      uint32_t nameoff = be32toh(view.structure + pos + 8);
      if (nameoff >= view.strings_size) {
        return -1;
      }
      if (fdt_names_equal(name, view.strings + nameoff,
                          view.strings_size - nameoff)) {
        *data = view.structure + pos + 12;
        *size = len;
        return 0;
      }
    }
    pos = next;
  }
  return -1;
}

int fdt_property_string(const void* blob, const struct fdt_node* node,
                        const char* property, const char** value,
                        size_t* size) {
  const void* data;
  size_t length, string_length;
  if (value == 0 || size == 0 ||
      fdt_get_property(blob, node, property, &data, &length) != 0 ||
      string_length_bounded((const char*)data, length, &string_length) != 0 ||
      string_length == 0 || string_length != length) {
    return -1;
  }
  *size = string_length - 1;
  *value = (const char*)data;
  return 0;
}

int fdt_property_string_is(const void* blob, const struct fdt_node* node,
                           const char* property, const char* expected) {
  const char* value;
  size_t value_size, expected_size, i;
  if (expected == 0 ||
      fdt_property_string(blob, node, property, &value, &value_size) != 0) {
    return 0;
  }
  expected_size = string_length(expected);
  if (value_size != expected_size) {
    return 0;
  }
  for (i = 0; i < value_size; ++i) {
    if (value[i] != expected[i]) {
      return 0;
    }
  }
  return 1;
}

int fdt_property_u32(const void* blob, const struct fdt_node* node,
                     const char* name, uint32_t* value) {
  const void* data;
  size_t size;
  if (value == 0 || fdt_get_property(blob, node, name, &data, &size) != 0 ||
      size != sizeof(uint32_t)) {
    return -1;
  }
  *value = be32toh((const uint8_t*)data);
  return 0;
}

int fdt_property_u64(const void* blob, const struct fdt_node* node,
                     const char* name, uint64_t* value) {
  const void* data;
  size_t size;
  if (value == 0 || fdt_get_property(blob, node, name, &data, &size) != 0 ||
      size != sizeof(uint64_t)) {
    return -1;
  }
  *value = be64toh((const uint8_t*)data);
  return 0;
}

int fdt_property_i32(const void* blob, const struct fdt_node* node,
                     const char* name, int32_t* value) {
  uint32_t raw;
  if (value == 0 || fdt_property_u32(blob, node, name, &raw) != 0) {
    return -1;
  }
  *value = (int32_t)raw;
  return 0;
}

int fdt_property_i64(const void* blob, const struct fdt_node* node,
                     const char* name, int64_t* value) {
  uint64_t raw;
  if (value == 0 || fdt_property_u64(blob, node, name, &raw) != 0) {
    return -1;
  }
  *value = (int64_t)raw;
  return 0;
}
