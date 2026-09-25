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
#include <proc/capabilities.h>
#include <proc/elf_loader.h>
#include <proc/init.h>
#include <proc/process.h>
#include <proc/task.h>
#include <sched/sched.h>
#include <smp/smp.h>
#include "ipc/channel.h"

#define _CONSOLE_LOG_SUBJECT "kmain"

static void init_userspace(void) {
  const void* initd_bin = NULL;
  size_t initd_size = 0;
  if (initramfs_find_file("initd", &initd_bin, &initd_size) != 0) {
    kpanic("Failed to find init task in initramfs.\n");
  }

  struct process* proc = process_create();
  if (proc == NULL) {
    kpanic("Failed to create initd process.\n");
  }

  struct elf_load_result load_result;
  if (elf_load_bootstrap(proc, initd_bin, initd_size, &load_result) !=
      ELF_LOAD_OK) {
    kpanic("Failed to load bootstrap initd ELF binary.\n");
  }

  struct task* task = task_create(proc, load_result.entry_point, 0);
  if (task == NULL) {
    kpanic("Failed to create initd task.\n");
  }

  struct capability cap_vspace = {
      .type = CAP_TYPE_VSPACE,
      .rights = CAP_RIGHT_READ | CAP_RIGHT_WRITE | CAP_RIGHT_GRANT,
      .badge = proc->pid,
      .object = proc,
  };
  cap_table_insert(&proc->capabilities, CAP_SLOT_VSPACE, &cap_vspace, NULL);

  struct capability cap_cnode = {
      .type = CAP_TYPE_CNODE,
      .rights = CAP_RIGHT_READ | CAP_RIGHT_WRITE | CAP_RIGHT_GRANT,
      .badge = proc->pid,
      .object = &proc->capabilities,
  };
  cap_table_insert(&proc->capabilities, CAP_SLOT_CNODE, &cap_cnode, NULL);

  struct capability cap_task = {
      .type = CAP_TYPE_TASK,
      .rights =
          CAP_RIGHT_READ | CAP_RIGHT_WRITE | CAP_RIGHT_CALL | CAP_RIGHT_GRANT,
      .badge = task->tid,
      .object = task,
  };
  cap_table_insert(&proc->capabilities, CAP_SLOT_TASK, &cap_task, NULL);

  console_log(_CONSOLE_LOG_SUBJECT,
              "Scheduling init task (tid=%zu, entry=0x%zx).\n", task->tid,
              (size_t)load_result.entry_point);
  sched_schedule(task);
}

void kmain(struct entry_info entry_info) {
  console_log(_CONSOLE_LOG_SUBJECT, "Entered kmain.\n");

  init_virtual_memory(entry_info.device_discovery);

  initramfs_load(&entry_info.device_discovery);

  smp_init(&entry_info.device_discovery, entry_info.boot_cpu_hardware_id);

  init_proc_system();
  init_channels();

  sched_init();

  init_userspace();

  sched_start();

  console_log(_CONSOLE_LOG_SUBJECT, "Reached end of kmain.\n");

  arch_panic_halt();
}
