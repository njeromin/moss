#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <arch/memory/layout.h>
#include <common/fdt.h>
#include <common/memory.h>
#include <core/console.h>
#include <initramfs/cpio_newc.h>
#include <initramfs/initramfs.h>

#define _CONSOLE_LOG_SUBJECT "initramfs"

static uintptr_t s_initramfs_base = 0;
static size_t s_initramfs_size = 0;

static uint32_t parse_hex8(const char hex[8]) {
  uint32_t val = 0;
  for (size_t i = 0; i < 8; ++i) {
    char c = hex[i];
    uint32_t digit = 0;
    if (c >= '0' && c <= '9') {
      digit = (uint32_t)(c - '0');
    } else if (c >= 'a' && c <= 'f') {
      digit = (uint32_t)(c - 'a' + 10);
    } else if (c >= 'A' && c <= 'F') {
      digit = (uint32_t)(c - 'A' + 10);
    } else {
      return 0;
    }
    val = (val << 4) | digit;
  }
  return val;
}

static bool check_magic(const char magic[CPIO_NEWC_MAGIC_SIZE]) {
  return magic[0] == '0' && magic[1] == '7' && magic[2] == '0' &&
         magic[3] == '7' && magic[4] == '0' && magic[5] == '1';
}

static bool string_equals(const char* a, const char* b) {
  if (a == NULL || b == NULL) {
    return a == b;
  }
  while (*a != '\0' && *b != '\0') {
    if (*a != *b) {
      return false;
    }
    a++;
    b++;
  }
  return *a == *b;
}
int get_initramfs_addr(const struct device_discovery_info* discovery,
                       uintptr_t* out_addr, size_t* out_size) {
  if (discovery == NULL || out_addr == NULL) {
    return -1;
  }

  if (discovery->dtb_phys == 0 || discovery->dtb_size == 0) {
    return -1;
  }

  const void* dtb = (const void*)arch_phys_to_virt(discovery->dtb_phys);
  if (fdt_check_full(dtb, discovery->dtb_size) != 0) {
    return -1;
  }

  struct fdt_node root;
  if (fdt_root(dtb, &root) != 0) {
    return -1;
  }

  struct fdt_node child;
  if (fdt_first_child(dtb, &root, &child) != 0) {
    return -1;
  }

  bool found_chosen = false;
  do {
    if (fdt_node_name_is(dtb, &child, "chosen")) {
      found_chosen = true;
      break;
    }
  } while (fdt_next_sibling(dtb, &child, &child) == 0);

  if (!found_chosen) {
    return -1;
  }

  uint64_t start = 0;
  uint64_t end = 0;
  if (fdt_property_u64(dtb, &child, "linux,initrd-start", &start) != 0 ||
      fdt_property_u64(dtb, &child, "linux,initrd-end", &end) != 0) {
    return -1;
  }

  if (end <= start) {
    return -1;
  }

  *out_addr = arch_phys_to_virt((uintptr_t)start);
  if (out_size != NULL) {
    *out_size = (size_t)(end - start);
  }

  return 0;
}

int initramfs_load(const struct device_discovery_info* device_discovery_info) {
  if (get_initramfs_addr(device_discovery_info, &s_initramfs_base,
                         &s_initramfs_size) != 0) {
    console_log(_CONSOLE_LOG_SUBJECT, "No initramfs found in device tree.\n");
    return -1;
  }

  console_log(_CONSOLE_LOG_SUBJECT,
              "Found initramfs at virtual 0x%zx (size %zu bytes).\n",
              s_initramfs_base, s_initramfs_size);

  if (s_initramfs_size < sizeof(struct cpio_newc_header)) {
    console_log(_CONSOLE_LOG_SUBJECT, "Initramfs too small for CPIO header.\n");
    return -1;
  }

  const struct cpio_newc_header* header =
      (const struct cpio_newc_header*)s_initramfs_base;
  if (!check_magic(header->c_magic)) {
    console_log(_CONSOLE_LOG_SUBJECT, "Invalid CPIO magic in initramfs.\n");
    return -1;
  }

  size_t offset = 0;
  size_t file_count = 0;
  while (offset + sizeof(struct cpio_newc_header) <= s_initramfs_size) {
    const struct cpio_newc_header* hdr =
        (const struct cpio_newc_header*)(s_initramfs_base + offset);
    if (!check_magic(hdr->c_magic)) {
      break;
    }

    uint32_t namesize = parse_hex8(hdr->c_namesize);
    uint32_t filesize = parse_hex8(hdr->c_filesize);
    if (namesize < 1) {
      break;
    }

    size_t name_offset = offset + sizeof(struct cpio_newc_header);
    size_t data_offset = align_up(name_offset + namesize, 4);
    size_t next_offset = align_up(data_offset + filesize, 4);

    if (data_offset + filesize > s_initramfs_size || next_offset < offset) {
      break;
    }

    const char* filename = (const char*)(s_initramfs_base + name_offset);
    if (string_equals(filename, CPIO_NEWC_TRAILER)) {
      break;
    }

    console_log(_CONSOLE_LOG_SUBJECT, "Entry: \"%s\" (size %zu bytes)\n",
                filename, (size_t)filesize);
    file_count++;

    offset = next_offset;
  }

  console_log(_CONSOLE_LOG_SUBJECT, "Loaded initramfs (%zu files).\n",
              file_count);
  return 0;
}

int initramfs_find_file(const char* name, const void** out_data,
                        size_t* out_size) {
  if (s_initramfs_base == 0 || s_initramfs_size == 0 || name == NULL ||
      out_data == NULL) {
    return -1;
  }

  size_t offset = 0;
  while (offset + sizeof(struct cpio_newc_header) <= s_initramfs_size) {
    const struct cpio_newc_header* hdr =
        (const struct cpio_newc_header*)(s_initramfs_base + offset);
    if (!check_magic(hdr->c_magic)) {
      return -1;
    }

    uint32_t namesize = parse_hex8(hdr->c_namesize);
    uint32_t filesize = parse_hex8(hdr->c_filesize);
    if (namesize < 1) {
      return -1;
    }

    size_t name_offset = offset + sizeof(struct cpio_newc_header);
    size_t data_offset = align_up(name_offset + namesize, 4);
    size_t next_offset = align_up(data_offset + filesize, 4);

    if (data_offset + filesize > s_initramfs_size || next_offset < offset) {
      return -1;
    }

    const char* filename = (const char*)(s_initramfs_base + name_offset);
    if (string_equals(filename, CPIO_NEWC_TRAILER)) {
      break;
    }

    if (string_equals(filename, name)) {
      *out_data = (const void*)(s_initramfs_base + data_offset);
      if (out_size != NULL) {
        *out_size = (size_t)filesize;
      }
      return 0;
    }

    offset = next_offset;
  }

  return -1;
}
