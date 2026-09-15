#pragma once

#include <stddef.h>
#include <stdint.h>
#include <core/kmain.h>

// Discover enabled CPUs from the DTB after memory initialisation, on the boot
// CPU only. Stores a private CPU list; does not start secondary CPUs.
void smp_init(const struct device_discovery_info* discovery,
              cpu_hardware_id_t boot_cpu_hardware_id);

// Boot CPU only, after shared subsystem initialisation. Starts secondaries and
// waits for acknowledgement; secondaries wait for scheduler release.
void smp_start_secondary_cpus(void);

size_t smp_cpu_count(void);
void smp_request_reschedule(size_t cpu_id);
void smp_release_secondary_cpus(uintptr_t entry_address);
