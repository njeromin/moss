#pragma once

#include <stddef.h>
#include <sync/spinlock.h>

struct task;

struct run_queue_node {
  struct task* task;
  struct run_queue_node* prev;
  struct run_queue_node* next;
};

struct run_queue {
  struct spinlock lock;
  size_t node_count;
  struct run_queue_node* head;
  struct run_queue_node* tail;
};

void run_queue_init(struct run_queue* rq);

void run_queue_push(struct run_queue* rq, struct task* task);

struct task* run_queue_pop(struct run_queue* rq);
