#include <smp/smp.h>

#include <stdbool.h>
#include <arch/memory/layout.h>
#include <arch/smp.h>
#include <arch/timer.h>
#include <common/fdt.h>
#include <core/console.h>
#include <core/panic.h>
#include <memory/pmm.h>

enum cpu_core_type {
  CPU_CORE_UNKNOWN,
  CPU_CORE_PERFORMANCE,
  CPU_CORE_EFFICIENCY,
};

struct cpu {
  size_t id;
  cpu_hardware_id_t hardware_id;
  enum cpu_core_type core_type;

  // Optional firmware metadata. Zero means unknown, not zero performance.
  // This is performance per MHz, not a frequency-adjusted scheduler capacity.
  uint32_t capacity_dmips_mhz;
  // Firmware-reported frequency, not a live or necessarily maximum frequency.
  uint64_t clock_frequency_hz;
  // Set by the target after local setup. Parked CPUs are not schedulable yet.
  bool started;
};

static struct cpu* cpus;
static struct cpu* boot_cpu;
static size_t cpu_count;
static bool startup_requested;
static void (*secondary_entry_fn)(void);
static bool secondary_cpus_released;

static bool cpu_node_enabled(const void* dtb, const struct fdt_node* node) {
  if (!fdt_property_string_is(dtb, node, "device_type", "cpu")) {
    return false;
  }

  const void* status;
  size_t status_size;
  return fdt_get_property(dtb, node, "status", &status, &status_size) != 0 ||
         fdt_property_string_is(dtb, node, "status", "okay") ||
         fdt_property_string_is(dtb, node, "status", "ok");
}

void smp_init(const struct device_discovery_info* discovery,
              cpu_hardware_id_t boot_cpu_hardware_id) {
  if (cpus != NULL) {
    kpanic("SMP discovery already initialised.\n");
  }

  if (discovery == NULL || discovery->dtb_phys == 0) {
    kpanic("SMP discovery requires a device tree.\n");
  }

  const void* dtb = (const void*)arch_phys_to_virt(discovery->dtb_phys);
  if (fdt_check_full(dtb, discovery->dtb_size) != 0) {
    kpanic("Invalid device tree for SMP discovery.\n");
  }

  struct fdt_node root;
  struct fdt_node parent;
  if (fdt_root(dtb, &root) != 0 || fdt_first_child(dtb, &root, &parent) != 0) {
    kpanic("Device tree has no CPUs node.\n");
  }

  do {
    if (fdt_node_name_is(dtb, &parent, "cpus")) {
      break;
    }
    if (fdt_next_sibling(dtb, &parent, &parent) != 0) {
      kpanic("Device tree has no CPUs node.\n");
    }
  } while (1);

  uint32_t address_cells;
  uint32_t size_cells;
  if (fdt_property_u32(dtb, &parent, "#address-cells", &address_cells) != 0 ||
      (address_cells != 1 && address_cells != 2) ||
      fdt_property_u32(dtb, &parent, "#size-cells", &size_cells) != 0 ||
      size_cells != 0) {
    kpanic("Unsupported CPU address layout in device tree.\n");
  }

  struct fdt_node node;
  size_t count = 0;
  if (fdt_first_child(dtb, &parent, &node) == 0) {
    do {
      if (cpu_node_enabled(dtb, &node)) {
        if (count == SIZE_MAX / sizeof(*cpus)) {
          kpanic("CPU discovery allocation size overflow.\n");
        }
        ++count;
      }
    } while (fdt_next_sibling(dtb, &node, &node) == 0);
  }

  if (count == 0) {
    kpanic("Device tree has no enabled CPUs.\n");
  }

  size_t bytes = count * sizeof(*cpus);
  size_t frames = bytes / PMM_FRAME_SIZE + (bytes % PMM_FRAME_SIZE != 0);
  uintptr_t phys;
  if (pmm_alloc(frames, &phys) != PMM_OK) {
    kpanic("Failed to allocate CPU discovery storage.\n");
  }
  cpus = (struct cpu*)arch_phys_to_virt(phys);

  if (fdt_first_child(dtb, &parent, &node) == 0) {
    do {
      if (!cpu_node_enabled(dtb, &node)) {
        continue;
      }

      uint64_t hardware_id;
      if (address_cells == 1) {
        uint32_t value;
        if (fdt_property_u32(dtb, &node, "reg", &value) != 0) {
          kpanic("Invalid CPU hardware ID in device tree.\n");
        }
        hardware_id = value;
      } else {
        if (fdt_property_u64(dtb, &node, "reg", &hardware_id) != 0) {
          kpanic("Invalid CPU hardware ID in device tree.\n");
        }
      }

      cpu_hardware_id_t cpu_id = (cpu_hardware_id_t)hardware_id;
      if ((uint64_t)cpu_id != hardware_id) {
        kpanic("CPU hardware ID exceeds architecture width.\n");
      }

      for (size_t i = 0; i < cpu_count; ++i) {
        if (cpus[i].hardware_id == cpu_id) {
          kpanic("Duplicate CPU hardware ID in device tree.\n");
        }
      }

      cpus[cpu_count] = (struct cpu){
          .id = cpu_count,
          .hardware_id = cpu_id,
          // Generic DT properties cannot establish a P-core/E-core role.
          .core_type = CPU_CORE_UNKNOWN,
      };
      if (cpu_id == boot_cpu_hardware_id) {
        boot_cpu = &cpus[cpu_count];
      }

      uint32_t capacity;
      if (fdt_property_u32(dtb, &node, "capacity-dmips-mhz", &capacity) == 0) {
        cpus[cpu_count].capacity_dmips_mhz = capacity;
      }

      uint32_t frequency32;
      uint64_t frequency64;
      if (fdt_property_u32(dtb, &node, "clock-frequency", &frequency32) == 0) {
        cpus[cpu_count].clock_frequency_hz = frequency32;
      } else if (fdt_property_u64(dtb, &node, "clock-frequency",
                                  &frequency64) == 0) {
        cpus[cpu_count].clock_frequency_hz = frequency64;
      }
      ++cpu_count;
    } while (fdt_next_sibling(dtb, &node, &node) == 0);
  }

  if (boot_cpu == NULL) {
    kpanic("Boot CPU missing from enabled device tree CPUs.\n");
  }

  console_log("smp", "Discovered %zu CPUs.\n", cpu_count);
  console_log("smp", "Boot CPU has logical ID %zu.\n", boot_cpu->id);
  arch_cpu_set_logical_id(boot_cpu->id);
  boot_cpu->started = true;
}

static _Noreturn void secondary_entry(size_t id) {
  if (id >= cpu_count || arch_cpu_logical_id() != id || &cpus[id] == boot_cpu) {
    kpanic("Invalid secondary CPU identity.\n");
  }
  // Publish only after architecture setup and entry onto this CPU's stack.
  __atomic_store_n(&cpus[id].started, true, __ATOMIC_RELEASE);
  while (!__atomic_load_n(&secondary_cpus_released, __ATOMIC_ACQUIRE)) {
    arch_cpu_relax();
  }
  if (secondary_entry_fn != NULL) {
    secondary_entry_fn();
  }
  while (true) {
    arch_cpu_idle();
  }
  kpanic("Secondary CPU entry returned.\n");
}

void smp_start_secondary_cpus(void (*entry_fn)(void)) {
  if (boot_cpu == NULL || startup_requested ||
      arch_cpu_logical_id() != boot_cpu->id) {
    kpanic("Invalid SMP startup request.\n");
  }
  startup_requested = true;
  secondary_entry_fn = entry_fn;

  for (size_t i = 0; i < cpu_count; ++i) {
    struct cpu* cpu = &cpus[i];
    if (cpu == boot_cpu) {
      continue;
    }

    uintptr_t stack_phys;
    const size_t stack_frames = 4;
    if (pmm_alloc(stack_frames, &stack_phys) != PMM_OK) {
      kpanic("Failed to allocate secondary CPU stack.\n");
    }
    uintptr_t stack_top =
        arch_phys_to_virt(stack_phys) + stack_frames * PMM_FRAME_SIZE;

    struct cpu_start_info info = {
        .logical_id = cpu->id,
        .stack_top = stack_top,
        .entry = secondary_entry,
    };

    if (arch_smp_start_cpu(cpu->hardware_id, &info) != 0) {
      kpanic("Failed to start CPU %zu.\n", cpu->id);
    }
    uint64_t start = arch_timer_get_ticks();
    uint64_t timeout = arch_timer_nanos_to_ticks(5000000000ULL);

    while (!__atomic_load_n(&cpu->started, __ATOMIC_ACQUIRE)) {
      if (arch_timer_get_ticks() - start >= timeout) {
        kpanic("Timed out starting CPU %zu.\n", cpu->id);
      }

      arch_cpu_relax();
    }

    console_log("smp", "CPU %zu started.\n", cpu->id);
  }

  arch_smp_finish_startup();
  __atomic_store_n(&secondary_cpus_released, true, __ATOMIC_RELEASE);
  console_log("smp", "All %zu CPUs acknowledged startup.\n", cpu_count);
}

size_t smp_cpu_count(void) {
  return cpu_count;
}

void smp_request_reschedule(size_t cpu_id) {
  if (cpu_id >= cpu_count) {
    kpanic("Invalid reschedule target.\n");
  }
  if (!__atomic_load_n(&cpus[cpu_id].started, __ATOMIC_ACQUIRE)) {
    return;
  }
  arch_smp_reschedule_cpu(cpus[cpu_id].hardware_id);
}
