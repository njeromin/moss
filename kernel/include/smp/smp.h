#pragma once

#include <core/kmain.h>

// Discover enabled CPUs from the DTB after memory initialisation, on the boot
// CPU only. Stores a private CPU list; does not start secondary CPUs.
void smp_init(const struct device_discovery_info* discovery,
              cpu_hardware_id_t boot_cpu_hardware_id);

// Boot CPU only, after shared subsystem initialisation. Starts secondaries and
// waits for acknowledgement; they remain parked pending scheduler integration.
void smp_start_secondary_cpus(void);
