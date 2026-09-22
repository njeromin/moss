#include <core/kmain.h>

#include <arch/memory/layout.h>
#include <arch/panic.h>
#include <common/memory.h>
#include <core/console.h>
#include <core/panic.h>
#include <memory/init.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <initramfs/initramfs.h>
#include <proc/init.h>
#include <proc/process.h>
#include <proc/task.h>
#include <sched/sched.h>
#include <smp/smp.h>

#define _CONSOLE_LOG_SUBJECT "kmain"

void kmain(struct entry_info entry_info) {
  console_log(_CONSOLE_LOG_SUBJECT, "Entered kmain.\n");

  init_virtual_memory(entry_info.device_discovery);

  initramfs_load(&entry_info.device_discovery);

  smp_init(&entry_info.device_discovery, entry_info.boot_cpu_hardware_id);

  init_proc_system();

  sched_init();

  smp_start_secondary_cpus();

  // Create test process and task
  struct process* proc = process_create();
  if (proc == NULL) {
    kpanic("Failed to create test process.\n");
  }

  // 1. addi a7, zero, 42 (0x02a00893), 2. ecall (0x00000073),
  // 3. ebreak (0x00100073), 4. j . (0x0000006f)
  static const uint32_t test_user_code[] = {
      0x02a00893,  // addi a7, zero, 42
      0x00000073,  // ecall
      0x00100073,  // ebreak
      0x0000006f,  // j . (jal x0, 0)
  };

  uintptr_t code_phys;
  if (pmm_alloc(1, &code_phys) != PMM_OK) {
    kpanic("Failed to allocate test code page.\n");
  }
  uintptr_t user_entry = 0x0000000010000000ULL;
  memory_set((void*)arch_phys_to_virt(code_phys), 0, PMM_FRAME_SIZE);
  memory_copy((void*)arch_phys_to_virt(code_phys), test_user_code,
              sizeof(test_user_code));

  if (vmm_map(&proc->memory_space, user_entry, code_phys, PMM_FRAME_SIZE,
              PAGE_USER | PAGE_READ | PAGE_EXEC) != VMM_OK) {
    kpanic("Failed to map test code page into user space.\n");
  }

  struct task* task1 = task_create(proc, user_entry, 0);
  struct task* task2 = task_create(proc, user_entry, 0);
  if (task1 == NULL || task2 == NULL) {
    kpanic("Failed to create test tasks.\n");
  }

  console_log(_CONSOLE_LOG_SUBJECT, "Scheduling test tasks (tid=%zu,%zu).\n",
              task1->tid, task2->tid);
  sched_schedule(task1);
  sched_schedule(task2);

  sched_start();

  console_log(_CONSOLE_LOG_SUBJECT, "Reached end of kmain.\n");
  arch_panic_halt();
}
