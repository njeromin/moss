#pragma once

#include <common/syscall.h>
#include <stddef.h>
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

// Initialise a capability table.
void cap_table_init(struct capability_table* table);

// Lookup a capability with required rights.
int cap_table_lookup(struct capability_table* table, cap_ptr_t slot,
                     enum cap_type expected_type, uint8_t required_rights,
                     struct capability* out_cap);

// Insert a capability into a slot (or find first free slot if slot == CAP_SLOT_ANY).
int cap_table_insert(struct capability_table* table, cap_ptr_t slot,
                     const struct capability* cap, cap_ptr_t* out_assigned_slot);

// Delete capability in a slot.
int cap_table_delete(struct capability_table* table, cap_ptr_t slot);

// Copy capability from src to dest table.
int cap_table_copy(struct capability_table* src_table, cap_ptr_t src_slot,
                   struct capability_table* dest_table, cap_ptr_t dest_slot,
                   uint8_t rights_mask, uint64_t badge,
                   cap_ptr_t* out_assigned_slot);
