#pragma once

#include <stdint.h>
#include <sync/spinlock.h>

#define CAP_TABLE_INITIAL_CAPACITY 64

typedef uint32_t cap_ptr_t;

enum cap_type {
  CAP_TYPE_NONE = 0,
  CAP_TYPE_ENDPOINT,
  CAP_TYPE_TASK,
  CAP_TYPE_VSPACE,
  CAP_TYPE_FRAME,
  CAP_TYPE_CNODE,
};

enum cap_rights {
  CAP_RIGHT_NONE = 0,
  CAP_RIGHT_READ = (1u << 0),
  CAP_RIGHT_WRITE = (1u << 1),
  CAP_RIGHT_CALL = (1u << 2),
  CAP_RIGHT_GRANT = (1u << 3),
};

struct capability {
  enum cap_type type;
  uint8_t rights;
  uint64_t badge;
  void* object;
};

struct capability_table {
  struct spinlock lock;
  size_t ref_count;  // Number of tasks sharing this table
  size_t capacity;
  struct capability slots[CAP_TABLE_INITIAL_CAPACITY];
};
