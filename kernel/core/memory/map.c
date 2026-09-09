#include <core/memory/map.h>

#include <core/console.h>
#include <core/panic.h>

#define _CONSOLE_LOG_SUBJECT "memory_map"

static struct memory_region available_regions[MAX_MEMORY_REGIONS];
static struct memory_region reserved_regions[MAX_MEMORY_REGIONS];

static struct memory_map global_memory_map = {
    .available = available_regions,
    .available_capacity = MAX_MEMORY_REGIONS,
    .available_count = 0,
    .reserved = reserved_regions,
    .reserved_capacity = MAX_MEMORY_REGIONS,
    .reserved_count = 0,
    .min_phys_addr = 0,
    .max_phys_addr = 0,
};

int memory_map_init(const struct device_discovery_info device_discovery) {
  int res = arch_get_memory_map(device_discovery, &global_memory_map);
  if (res != ARCH_MEMORY_MAP_OK) {
    switch (res) {
      case ARCH_MEMORY_MAP_INVALID:
        kpanic("Encountered invalid map while getting memory map.\n");
        break;
      case ARCH_MEMORY_MAP_NO_SPACE:
        kpanic("No space while getting memory map.\n");
        break;
      default:
        kpanic("Unexpected error while getting memory map.\n");
        break;
    }
    return -1;
  }

  console_log(_CONSOLE_LOG_SUBJECT, "Found %zu available memory regions.\n",
              global_memory_map.available_count);
  for (size_t i = 0; i < global_memory_map.available_count; i++) {
    struct memory_region region = global_memory_map.available[i];
    console_log(_CONSOLE_LOG_SUBJECT, "-> %p (%zu bytes)\n",
                (void*)region.offset, region.size);
  }

  console_log(_CONSOLE_LOG_SUBJECT, "Found %zu reserved memory regions.\n",
              global_memory_map.reserved_count);
  for (size_t i = 0; i < global_memory_map.reserved_count; i++) {
    struct memory_region region = global_memory_map.reserved[i];
    console_log(_CONSOLE_LOG_SUBJECT, "-> %p (%zu bytes)\n",
                (void*)region.offset, region.size);
  }

  console_log(_CONSOLE_LOG_SUBJECT, "Lowest address found:  %p.\n",
              (void*)global_memory_map.min_phys_addr);
  console_log(_CONSOLE_LOG_SUBJECT, "Highest address found: %p.\n",
              (void*)global_memory_map.max_phys_addr);

  return 0;
}

const struct memory_map* memory_map_get(void) {
  return &global_memory_map;
}

int memory_map_add_reserved(struct memory_region region) {
  if (global_memory_map.reserved_count >= global_memory_map.reserved_capacity) {
    return -1;
  }
  global_memory_map.reserved[global_memory_map.reserved_count++] = region;
  return 0;
}
