#pragma once

#include <sync/spinlock.h>

struct task;

struct ipc_endpoint {
  struct spinlock lock;

  // Senders waiting for a receiver to arrive
  struct task* send_queue_head;
  struct task* send_queue_tail;

  // Receivers waiting for a sender to arrive
  struct task* recv_queue_head;
  struct task* recv_queue_tail;
};

void ipc_endpoint_init(struct ipc_endpoint* ep);
