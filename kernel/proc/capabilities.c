#include <proc/capabilities.h>

#include <common/memory.h>
#include <common/syscall.h>

void cap_table_init(struct capability_table* table) {
  if (table == NULL) {
    return;
  }

  spinlock_init(&table->lock);
  table->ref_count = 1;
  table->capacity = CAP_TABLE_INITIAL_CAPACITY;
  memory_set(table->slots, 0, sizeof(table->slots));
}

int cap_table_lookup(struct capability_table* table, cap_ptr_t slot,
                     enum cap_type expected_type, uint8_t required_rights,
                     struct capability* out_cap) {
  if (table == NULL || out_cap == NULL) {
    return SYS_ERR_INVALID_ARG;
  }

  if (slot >= table->capacity) {
    return SYS_ERR_INVALID_CAP;
  }

  bool irq = spin_lock_irqsave(&table->lock);

  struct capability* entry = &table->slots[slot];
  if (entry->type == CAP_TYPE_NONE) {
    spin_unlock_irqrestore(&table->lock, irq);
    return SYS_ERR_INVALID_CAP;
  }

  if (expected_type != CAP_TYPE_NONE && entry->type != expected_type) {
    spin_unlock_irqrestore(&table->lock, irq);
    return SYS_ERR_INVALID_CAP;
  }

  if ((entry->rights & required_rights) != required_rights) {
    spin_unlock_irqrestore(&table->lock, irq);
    return SYS_ERR_RIGHTS;
  }

  *out_cap = *entry;

  spin_unlock_irqrestore(&table->lock, irq);
  return SYS_OK;
}

int cap_table_insert(struct capability_table* table, cap_ptr_t slot,
                     const struct capability* cap,
                     cap_ptr_t* out_assigned_slot) {
  if (table == NULL || cap == NULL) {
    return SYS_ERR_INVALID_ARG;
  }

  bool irq = spin_lock_irqsave(&table->lock);

  cap_ptr_t target_slot = slot;
  if (slot == CAP_SLOT_ANY) {
    bool found = false;
    // Don't assign slot 0 (reserved for null cap) if possible
    for (size_t i = 1; i < table->capacity; i++) {
      if (table->slots[i].type == CAP_TYPE_NONE) {
        target_slot = (cap_ptr_t)i;
        found = true;
        break;
      }
    }
    if (!found) {
      spin_unlock_irqrestore(&table->lock, irq);
      return SYS_ERR_NO_SLOT;
    }
  } else {
    if (target_slot >= table->capacity) {
      spin_unlock_irqrestore(&table->lock, irq);
      return SYS_ERR_INVALID_ARG;
    }
    if (table->slots[target_slot].type != CAP_TYPE_NONE) {
      spin_unlock_irqrestore(&table->lock, irq);
      return SYS_ERR_ALREADY_MAPPED;
    }
  }

  table->slots[target_slot] = *cap;
  if (out_assigned_slot != NULL) {
    *out_assigned_slot = target_slot;
  }

  spin_unlock_irqrestore(&table->lock, irq);
  return SYS_OK;
}

int cap_table_delete(struct capability_table* table, cap_ptr_t slot) {
  if (table == NULL || slot >= table->capacity) {
    return SYS_ERR_INVALID_ARG;
  }

  bool irq = spin_lock_irqsave(&table->lock);

  table->slots[slot].type = CAP_TYPE_NONE;
  table->slots[slot].rights = 0;
  table->slots[slot].badge = 0;
  table->slots[slot].object = NULL;

  spin_unlock_irqrestore(&table->lock, irq);
  return SYS_OK;
}

int cap_table_copy(struct capability_table* src_table, cap_ptr_t src_slot,
                   struct capability_table* dest_table, cap_ptr_t dest_slot,
                   uint8_t rights_mask, uint64_t badge,
                   cap_ptr_t* out_assigned_slot) {
  if (src_table == NULL || dest_table == NULL) {
    return SYS_ERR_INVALID_ARG;
  }

  struct capability src_cap;
  int lookup_res =
      cap_table_lookup(src_table, src_slot, CAP_TYPE_NONE, 0, &src_cap);
  if (lookup_res != SYS_OK) {
    return lookup_res;
  }

  struct capability new_cap = src_cap;
  if (rights_mask != 0) {
    new_cap.rights &= rights_mask;
  }
  if (badge != 0) {
    new_cap.badge = badge;
  }

  return cap_table_insert(dest_table, dest_slot, &new_cap, out_assigned_slot);
}
