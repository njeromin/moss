#pragma once

#include <arch/cpu.h>
#include <stddef.h>
#include <stdint.h>

struct device_discovery_info {
  // Physical address of the DTB.
  uintptr_t dtb_phys;
  size_t dtb_size;

  uintptr_t acpi_rsdp_phys;
};

struct entry_info {
  struct device_discovery_info device_discovery;
  cpu_hardware_id_t boot_cpu_hardware_id;
};

void kmain(struct entry_info entry_info);
