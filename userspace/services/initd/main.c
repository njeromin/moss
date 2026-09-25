#include <syscall.h>

static volatile int child_executed = 0;

static void child_entry(uintptr_t arg) {
  child_executed = (int)arg;
  sys_exit(0);
}

int main(void) {
  // Test 1: Frame allocation
  uint32_t frame_slot = 0;
  int err = sys_frame_alloc(1, CAP_SLOT_ANY, &frame_slot);
  if (err != SYS_OK) {
    return 1;
  }

  // Test 2: Map frame into self VSpace at 0x20000
  uintptr_t test_vaddr = 0x00020000ULL;
  err = sys_vspace_map(CAP_SLOT_VSPACE, frame_slot, test_vaddr,
                       PROT_USER | PROT_READ | PROT_WRITE);
  if (err != SYS_OK) {
    return 2;
  }

  // Test 3: Read and write to the mapped memory page
  volatile uint32_t* ptr = (volatile uint32_t*)test_vaddr;
  *ptr = 0xdeadbeef;
  if (*ptr != 0xdeadbeef) {
    return 3;
  }

  // Test 4: Unmap virtual memory range
  err = sys_vspace_unmap(CAP_SLOT_VSPACE, test_vaddr, 4096);
  if (err != SYS_OK) {
    return 4;
  }

  // Test 5: Free physical frame
  err = sys_frame_free(frame_slot);
  if (err != SYS_OK) {
    return 5;
  }

  // Test 6: Create a child VSpace
  uint32_t child_vspace_slot = 0;
  err = sys_vspace_create(CAP_SLOT_ANY, &child_vspace_slot);
  if (err != SYS_OK) {
    return 6;
  }

  // Test 7: Destroy child VSpace
  err = sys_vspace_destroy(child_vspace_slot);
  if (err != SYS_OK) {
    return 7;
  }

  // Test 8: Task creation and execution
  uint32_t task_slot = 0;
  err = sys_task_create(CAP_SLOT_VSPACE, (uintptr_t)child_entry, 123,
                        CAP_SLOT_ANY, &task_slot);
  if (err != SYS_OK) {
    return 8;
  }

  // Test 9: Start the created task
  err = sys_task_start(task_slot, 10);
  if (err != SYS_OK) {
    return 9;
  }

  // Test 10: Wait for child task to execute
  while (child_executed != 123) {
    sys_yield();
  }

  // All tests passed successfully!
  return 0;
}
