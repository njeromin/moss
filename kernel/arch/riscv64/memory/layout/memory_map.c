#include <arch/memory/layout.h>

#include <common/endian.h>
#include <common/fdt.h>

static int add_available(struct memory_map* memory_map,
                         struct memory_region region) {
  if (memory_map->available_count + 1 > memory_map->available_capacity) {
    return -1;
  }

  memory_map->available[memory_map->available_count++] = region;
  return 0;
}

static int add_reserved(struct memory_map* memory_map,
                        struct memory_region region) {
  if (memory_map->reserved_count + 1 > memory_map->reserved_capacity) {
    return -1;
  }

  memory_map->reserved[memory_map->reserved_count++] = region;
  return 0;
}

static int read_cells(const unsigned char* data, size_t size, size_t cells,
                      uint64_t* value) {
  size_t i;
  uint64_t result = 0;
  if (data == 0 || value == 0 || cells == 0 || cells > 2 ||
      size != cells * sizeof(uint32_t)) {
    return -1;
  }
  for (i = 0; i < cells; ++i) {
    result = (result << 32) | be32toh(data + i * sizeof(uint32_t));
  }
  *value = result;
  return 0;
}

static int add_reg_regions(const void* blob, const struct fdt_node* node,
                           size_t address_cells, size_t size_cells,
                           struct memory_map* memory_map, int reserved,
                           uint8_t flags) {
  const void* data;
  size_t size, entry_cells, offset;
  if (fdt_get_property(blob, node, "reg", &data, &size) != 0) {
    return 0;
  }
  entry_cells = address_cells + size_cells;
  if (entry_cells == 0 || entry_cells > 4 ||
      size % (entry_cells * sizeof(uint32_t)) != 0) {
    return -1;
  }
  for (offset = 0; offset < size; offset += entry_cells * sizeof(uint32_t)) {
    uint64_t address, length;
    const unsigned char* entry = (const unsigned char*)data + offset;
    if (read_cells(entry, address_cells * sizeof(uint32_t), address_cells,
                   &address) != 0 ||
        read_cells(entry + address_cells * sizeof(uint32_t),
                   size_cells * sizeof(uint32_t), size_cells, &length) != 0 ||
        length > UINTPTR_MAX - address) {
      return -1;
    }
    struct memory_region region = {
        .offset = (uintptr_t)address, .size = (size_t)length, .flags = flags};
    if (reserved) {
      if (add_reserved(memory_map, region) != 0) {
        return -2;
      }
    } else if (add_available(memory_map, region) != 0) {
      return -2;
    }
  }
  return 0;
}

static int memory_map_kernel(struct memory_map* memory_map) {
  uint64_t start = kernel_physical_start;
  uint64_t end = kernel_physical_end;
  struct memory_region region = {
      .offset = start,
      .size = end - start,
      .flags = MEMORY_REGION_RESERVED,
  };

  return add_reserved(memory_map, region);
}

static int memory_map_dtb(const void* dtb, struct memory_map* memory_map) {
  struct fdt_node root;
  struct fdt_node child;
  size_t address_cells = 2;
  size_t size_cells = 1;

  if (dtb == 0 || memory_map == 0 || fdt_root(dtb, &root) != 0) {
    return ARCH_MEMORY_MAP_INVALID;
  }

  {
    uint32_t value;
    if (fdt_property_u32(dtb, &root, "#address-cells", &value) == 0) {
      address_cells = value;
    }
    if (fdt_property_u32(dtb, &root, "#size-cells", &value) == 0) {
      size_cells = value;
    }
  }
  if (address_cells == 0 || address_cells > 2 || size_cells == 0 ||
      size_cells > 2) {
    return ARCH_MEMORY_MAP_INVALID;
  }

  if (fdt_first_child(dtb, &root, &child) == 0) {
    do {
      if (fdt_property_string_is(dtb, &child, "device_type", "memory")) {
        int result = add_reg_regions(dtb, &child, address_cells, size_cells,
                                     memory_map, 0, 0);
        if (result != 0) {
          return result == -2 ? ARCH_MEMORY_MAP_NO_SPACE
                              : ARCH_MEMORY_MAP_INVALID;
        }
      }
      {
        if (fdt_node_name_is(dtb, &child, "reserved-memory")) {
          size_t reserved_address_cells = address_cells;
          size_t reserved_size_cells = size_cells;
          uint32_t value;
          if (fdt_property_u32(dtb, &child, "#address-cells", &value) == 0) {
            reserved_address_cells = value;
          }
          if (fdt_property_u32(dtb, &child, "#size-cells", &value) == 0) {
            reserved_size_cells = value;
          }
          if (reserved_address_cells == 0 || reserved_address_cells > 2 ||
              reserved_size_cells == 0 || reserved_size_cells > 2) {
            return ARCH_MEMORY_MAP_INVALID;
          }
          struct fdt_node reserved_child;
          if (fdt_first_child(dtb, &child, &reserved_child) == 0) {
            do {
              uint8_t flags = MEMORY_REGION_RESERVED;
              const void* unused;
              size_t unused_size;
              if (fdt_get_property(dtb, &reserved_child, "no-map", &unused,
                                   &unused_size) == 0) {
                flags |= MEMORY_REGION_NO_MAP;
              }
              if (fdt_get_property(dtb, &reserved_child, "reusable", &unused,
                                   &unused_size) == 0) {
                flags |= MEMORY_REGION_REUSABLE;
              }
              int result =
                  add_reg_regions(dtb, &reserved_child, reserved_address_cells,
                                  reserved_size_cells, memory_map, 1, flags);
              if (result != 0) {
                return result == -2 ? ARCH_MEMORY_MAP_NO_SPACE
                                    : ARCH_MEMORY_MAP_INVALID;
              }
            } while (fdt_next_sibling(dtb, &reserved_child, &reserved_child) ==
                     0);
          }
        }
      }
    } while (fdt_next_sibling(dtb, &child, &child) == 0);
  }

  return ARCH_MEMORY_MAP_OK;
}

int arch_get_memory_map(
    const struct device_discovery_info device_discovery_info,
    struct memory_map* memory_map) {
  // NOTE: We can assume that the FDT is safe here since the FDT has already been checked once in arch entry.

  memory_map->available_count = 0;
  memory_map->reserved_count = 0;

  if (memory_map_kernel(memory_map) != ARCH_MEMORY_MAP_OK) {
    // TODO: Change this, this is handling error wrong.
    return ARCH_MEMORY_MAP_INVALID;
  }

  if (memory_map_dtb((void*)device_discovery_info.dtb, memory_map) !=
      ARCH_MEMORY_MAP_OK) {
    // TODO: Change this, this is handling error wrong.
    return ARCH_MEMORY_MAP_INVALID;
  }

  return ARCH_MEMORY_MAP_OK;
}
