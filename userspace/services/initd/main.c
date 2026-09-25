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

  // Test 11: Channel creation
  uint32_t ep_a = 0;
  uint32_t ep_b = 0;
  err = sys_channel_create(0, CAP_SLOT_ANY, CAP_SLOT_ANY, &ep_a, &ep_b);
  if (err != SYS_OK || ep_a == 0 || ep_b == 0) {
    return 11;
  }

  // Test 12: Channel write and read data
  const char test_str[] = "channel_ipc_works";
  err = sys_channel_write(ep_a, test_str, sizeof(test_str), NULL, 0);
  if (err != SYS_OK) {
    return 12;
  }

  uint32_t observed_sig = 0;
  err = sys_object_wait_one(ep_b, SIGNAL_READABLE, 0, &observed_sig);
  if (err != SYS_OK || !(observed_sig & SIGNAL_READABLE)) {
    return 13;
  }

  char read_buf[32] = {0};
  size_t actual_bytes = 0;
  size_t actual_caps = 0;
  err = sys_channel_read(ep_b, read_buf, sizeof(read_buf), &actual_bytes, NULL,
                         0, &actual_caps);
  if (err != SYS_OK || actual_bytes != sizeof(test_str)) {
    return 14;
  }

  for (size_t i = 0; i < sizeof(test_str); i++) {
    if (read_buf[i] != test_str[i]) {
      return 15;
    }
  }

  // Test 13: Capability transfer over channel
  uint32_t cap_frame = 0;
  err = sys_frame_alloc(1, CAP_SLOT_ANY, &cap_frame);
  if (err != SYS_OK) {
    return 16;
  }

  uint32_t send_caps[1] = {cap_frame};
  err = sys_channel_write(ep_b, "frame_pass", 10, send_caps, 1);
  if (err != SYS_OK) {
    return 17;
  }

  uint32_t recv_caps[1] = {0};
  err = sys_channel_read(ep_a, read_buf, sizeof(read_buf), &actual_bytes,
                         recv_caps, 1, &actual_caps);
  if (err != SYS_OK || actual_caps != 1 || recv_caps[0] == 0) {
    return 18;
  }

  // Map the received transferred frame
  uintptr_t transferred_vaddr = 0x00030000ULL;
  err = sys_vspace_map(CAP_SLOT_VSPACE, recv_caps[0], transferred_vaddr,
                       PROT_USER | PROT_READ | PROT_WRITE);
  if (err != SYS_OK) {
    return 19;
  }

  volatile uint32_t* trans_ptr = (volatile uint32_t*)transferred_vaddr;
  *trans_ptr = 0xcafebabe;
  if (*trans_ptr != 0xcafebabe) {
    return 20;
  }

  sys_vspace_unmap(CAP_SLOT_VSPACE, transferred_vaddr, 4096);
  sys_frame_free(recv_caps[0]);

  // Test 14: Peer closed signalling
  sys_cap_delete(ep_a);

  observed_sig = 0;
  err = sys_object_wait_one(ep_b, SIGNAL_PEER_CLOSED, 0, &observed_sig);
  if (!(observed_sig & SIGNAL_PEER_CLOSED)) {
    return 21;
  }

  sys_cap_delete(ep_b);

  // All tests passed successfully!
  return 0;
}
