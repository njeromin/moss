#include <arch/memory/layout.h>

#include <stdbool.h>
#include <common/endian.h>
#include <common/fdt.h>
#include <common/memory.h>

extern const uintptr_t bootstrap_physical_start;
extern const uintptr_t bootstrap_physical_end;

static uintptr_t find_free_ram(const struct memory_map* map, size_t size,
                               size_t align,
                               const struct memory_region* pending) {
  if (size == 0 || align == 0) {
    return 0;
  }

  for (size_t i = 0; i < map->available_count; ++i) {
    struct memory_region avail = map->available[i];
    if (avail.flags & (MEMORY_REGION_RESERVED | MEMORY_REGION_NO_MAP)) {
      continue;
    }

    uintptr_t start = avail.offset;
    // Always start searching above the kernel physical end to avoid firmware,
    // bootstrap memory, and kernel code/data.
    if (start < kernel_physical_end) {
      start = kernel_physical_end;
    }

    uintptr_t end = avail.offset + avail.size;
    if (start >= end) {
      continue;
    }

    while (start < end && start <= UINTPTR_MAX - (align - 1)) {
      uintptr_t candidate = align_up(start, align);
      if (candidate >= end || size > end - candidate) {
        break;
      }
      uintptr_t candidate_end = candidate + size;

      bool conflict = false;

      // Check conflict with pending region being added.
      if (pending != NULL && pending->size != 0) {
        uintptr_t pending_end = pending->offset + pending->size;
        if (candidate < pending_end && pending->offset < candidate_end) {
          if (pending_end > start) {
            start = pending_end;
          } else {
            break;
          }
          conflict = true;
        }
      }

      if (conflict) {
        continue;
      }

      // Check conflict with already reserved regions.
      for (size_t j = 0; j < map->reserved_count; ++j) {
        struct memory_region res = map->reserved[j];
        uintptr_t res_end = res.offset + res.size;
        if (res.size != 0 && candidate < res_end &&
            res.offset < candidate_end) {
          if (res_end > start) {
            start = res_end;
          } else {
            break;
          }
          conflict = true;
          break;
        }
      }

      if (!conflict) {
        return candidate;
      }
    }
  }
  return 0;
}

static int add_reserved(struct memory_map* memory_map,
                        struct memory_region region);

static int grow_reserved(struct memory_map* map,
                         const struct memory_region* pending);

static int grow_available(struct memory_map* map,
                          const struct memory_region* pending) {
  if (map->available_count == 0) {
    return -1;
  }

  size_t old_cap = map->available_capacity;
  size_t new_cap = (old_cap < 4) ? 4 : old_cap * 2;
  size_t alloc_size = align_up(new_cap * sizeof(struct memory_region), 4096);

  uintptr_t phys = find_free_ram(map, alloc_size, 4096, pending);
  if (phys == 0) {
    return -1;
  }

  struct memory_region* new_array =
      (struct memory_region*)arch_phys_to_virt(phys);
  memory_set(new_array, 0, new_cap * sizeof(struct memory_region));
  memory_copy(new_array, map->available,
              map->available_count * sizeof(struct memory_region));

  map->available = new_array;
  map->available_capacity = new_cap;

  struct memory_region self_res = {
      .offset = phys,
      .size = alloc_size,
      .flags = MEMORY_REGION_RESERVED,
  };
  return add_reserved(map, self_res);
}

static int grow_reserved(struct memory_map* map,
                         const struct memory_region* pending) {
  if (map->available_count == 0) {
    return -1;
  }

  size_t old_cap = map->reserved_capacity;
  size_t new_cap = (old_cap < 4) ? 4 : old_cap * 2;
  size_t alloc_size = align_up(new_cap * sizeof(struct memory_region), 4096);

  uintptr_t phys = find_free_ram(map, alloc_size, 4096, pending);
  if (phys == 0) {
    return -1;
  }

  struct memory_region* new_array =
      (struct memory_region*)arch_phys_to_virt(phys);
  memory_set(new_array, 0, new_cap * sizeof(struct memory_region));
  memory_copy(new_array, map->reserved,
              map->reserved_count * sizeof(struct memory_region));

  new_array[map->reserved_count++] = (struct memory_region){
      .offset = phys,
      .size = alloc_size,
      .flags = MEMORY_REGION_RESERVED,
  };

  if (phys < map->min_phys_addr) {
    map->min_phys_addr = phys;
  }
  if (phys + alloc_size > map->max_phys_addr) {
    map->max_phys_addr = phys + alloc_size;
  }

  map->reserved = new_array;
  map->reserved_capacity = new_cap;
  return 0;
}

static int add_available(struct memory_map* memory_map,
                         struct memory_region region) {
  if (memory_map->available_count >= memory_map->available_capacity) {
    if (grow_available(memory_map, &region) != 0) {
      return -1;
    }
  }

  memory_map->available[memory_map->available_count++] = region;

  if (region.offset < memory_map->min_phys_addr) {
    memory_map->min_phys_addr = region.offset;
  }

  size_t end = region.offset + region.size;
  if (end > memory_map->max_phys_addr) {
    memory_map->max_phys_addr = end;
  }

  return 0;
}

static int add_reserved(struct memory_map* memory_map,
                        struct memory_region region) {
  if (memory_map->reserved_count >= memory_map->reserved_capacity) {
    if (grow_reserved(memory_map, &region) != 0) {
      return -1;
    }
  }

  memory_map->reserved[memory_map->reserved_count++] = region;

  if (region.offset < memory_map->min_phys_addr) {
    memory_map->min_phys_addr = region.offset;
  }

  size_t end = region.offset + region.size;
  if (end > memory_map->max_phys_addr) {
    memory_map->max_phys_addr = end;
  }

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

  // Pass 1: Discover all available memory regions first, ensuring that any
  // dynamic table expansion has known RAM to allocate from.
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
    } while (fdt_next_sibling(dtb, &child, &child) == 0);
  }

  // Reserve the DTB blob itself.
  const unsigned char* blob = dtb;
  if (add_reserved(memory_map, (struct memory_region){
                                   .offset = (uintptr_t)dtb,
                                   .size = be32toh(blob + 4),
                                   .flags = MEMORY_REGION_RESERVED}) != 0) {
    return ARCH_MEMORY_MAP_NO_SPACE;
  }

  // The validated FDT reservation block is separate from /reserved-memory.
  const unsigned char* entry = blob + be32toh(blob + 16);
  for (;; entry += 16) {
    uint64_t address = be64toh(entry);
    uint64_t length = be64toh(entry + 8);
    if (address == 0 && length == 0) {
      break;
    }
    if (length > UINTPTR_MAX - address) {
      return ARCH_MEMORY_MAP_INVALID;
    }
    if (length != 0 &&
        add_reserved(memory_map, (struct memory_region){
                                     .offset = address,
                                     .size = length,
                                     .flags = MEMORY_REGION_RESERVED |
                                              MEMORY_REGION_FIRMWARE}) != 0) {
      return ARCH_MEMORY_MAP_NO_SPACE;
    }
  }

  // Pass 2: Discover all reserved-memory regions.
  if (fdt_first_child(dtb, &root, &child) == 0) {
    do {
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
  memory_map->min_phys_addr = SIZE_MAX;
  memory_map->max_phys_addr = 0;

  int res = ARCH_MEMORY_MAP_OK;

  res = memory_map_dtb((void*)device_discovery_info.dtb_phys, memory_map);
  if (res != ARCH_MEMORY_MAP_OK) {
    return ARCH_MEMORY_MAP_INVALID;
  }

  res = memory_map_kernel(memory_map);
  if (res != ARCH_MEMORY_MAP_OK) {
    return ARCH_MEMORY_MAP_INVALID;
  }

  if (add_reserved(memory_map, (struct memory_region){
                                   .offset = bootstrap_physical_start,
                                   .size = bootstrap_physical_end -
                                           bootstrap_physical_start,
                                   .flags = MEMORY_REGION_RESERVED}) != 0) {
    return ARCH_MEMORY_MAP_NO_SPACE;
  }

  return res;
}
