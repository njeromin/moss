#include <sched/sched.h>

#include <core/console.h>

#define _CONSOLE_LOG_SUBJECT "sched"

void sched_init(void) {
  console_log(_CONSOLE_LOG_SUBJECT, "Initialising scheduler.\n");
}
