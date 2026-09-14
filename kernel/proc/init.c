#include <proc/init.h>

#include <proc/capabilities.h>
#include <proc/process.h>
#include <proc/task.h>
#include "core/panic.h"

void init_proc_system() {
  if (processes_init() != 0) {
    kpanic("Failed to initialise processes system.");
  }

  if (tasks_init() != 0) {
    kpanic("Failed to initialise tasks system.");
  }
}
