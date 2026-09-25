#include <ipc/channel.h>

#include <arch/cpu.h>
#include <arch/memory/layout.h>
#include <asm/trap_layout.h>
#include <common/memory.h>
#include <common/syscall.h>
#include <memory/kmem_cache.h>
#include <memory/pmm.h>
#include <proc/capabilities.h>
#include <proc/task.h>
#include <sched/sched.h>
#include <sync/spinlock.h>

static struct kmem_cache channel_cache;
static struct kmem_cache message_cache;
static struct kmem_cache waiter_cache;

int init_channels(void) {
  int err = kmem_cache_init(&channel_cache, "channel_endpoint",
                            sizeof(struct channel_endpoint));
  if (err != 0) {
    return err;
  }

  err = kmem_cache_init(
      &message_cache, "channel_message",
      sizeof(struct channel_message) + CHANNEL_INLINE_MESSAGE_BYTES);
  if (err != 0) {
    return err;
  }

  return kmem_cache_init(&waiter_cache, "channel_waiter",
                         sizeof(struct channel_waiter));
}

static void free_channel_message(struct channel_message* msg) {
  if (msg == NULL) {
    return;
  }

  if (msg->is_external && msg->data != NULL) {
    uintptr_t phys = arch_virt_to_phys((virt_addr_t)msg->data);
    pmm_free((phys_addr_t)phys, 1);
    msg->data = NULL;
  }

  for (size_t i = 0; i < msg->num_caps; i++) {
    if (msg->caps[i].type == CAP_TYPE_FRAME) {
      phys_addr_t phys = (phys_addr_t)(uintptr_t)msg->caps[i].object;
      size_t count = (size_t)msg->caps[i].badge;
      pmm_free(phys, count);
    } else if (msg->caps[i].type == CAP_TYPE_CHANNEL) {
      channel_endpoint_close((struct channel_endpoint*)msg->caps[i].object);
    }
  }

  kmem_cache_free(&message_cache, msg);
}

static void wake_waiters_locked(struct channel_endpoint* ep,
                                uint32_t triggered_signals) {
  struct channel_waiter** curr = &ep->waiters_head;
  while (*curr != NULL) {
    struct channel_waiter* waiter = *curr;
    if ((waiter->interested_signals &
         (triggered_signals | SIGNAL_PEER_CLOSED)) != 0) {
      *curr = waiter->next;
      struct task* task = waiter->task;
      if (task != NULL && task->trap_frame != NULL) {
        task->trap_frame->a0 =
            (waiter->interested_signals & triggered_signals)
                ? (trap_register_t)SYS_OK
                : (trap_register_t)SYS_ERR_PEER_CLOSED;
        task->trap_frame->a1 = (trap_register_t)ep->signals;
      }
      kmem_cache_free(&waiter_cache, waiter);
      if (task != NULL) {
        sched_schedule(task);
      }
    } else {
      curr = &(*curr)->next;
    }
  }
}

static void lock_channel_pair(struct channel_endpoint* ep,
                              struct channel_endpoint** out_peer,
                              bool* out_irq_ep, bool* out_irq_peer) {
  for (;;) {
    *out_irq_ep = spin_lock_irqsave(&ep->lock);
    struct channel_endpoint* peer = ep->peer;
    if (peer == NULL) {
      *out_peer = NULL;
      return;
    }

    if ((uintptr_t)ep < (uintptr_t)peer) {
      *out_irq_peer = spin_lock_irqsave(&peer->lock);
      *out_peer = peer;
      return;
    }

    spin_unlock_irqrestore(&ep->lock, *out_irq_ep);
    *out_irq_peer = spin_lock_irqsave(&peer->lock);
    *out_irq_ep = spin_lock_irqsave(&ep->lock);
    if (ep->peer == peer) {
      *out_peer = peer;
      return;
    }

    spin_unlock_irqrestore(&ep->lock, *out_irq_ep);
    spin_unlock_irqrestore(&peer->lock, *out_irq_peer);
  }
}

static void unlock_channel_pair(struct channel_endpoint* ep,
                                struct channel_endpoint* peer, bool irq_ep,
                                bool irq_peer) {
  if (peer != NULL) {
    if ((uintptr_t)ep < (uintptr_t)peer) {
      spin_unlock_irqrestore(&peer->lock, irq_peer);
      spin_unlock_irqrestore(&ep->lock, irq_ep);
    } else {
      spin_unlock_irqrestore(&ep->lock, irq_ep);
      spin_unlock_irqrestore(&peer->lock, irq_peer);
    }
  } else {
    spin_unlock_irqrestore(&ep->lock, irq_ep);
  }
}

int channel_create_pair(struct channel_endpoint** out_ep0,
                        struct channel_endpoint** out_ep1) {
  if (out_ep0 == NULL || out_ep1 == NULL) {
    return SYS_ERR_INVALID_ARG;
  }

  struct channel_endpoint* ep0 = kmem_cache_zalloc(&channel_cache);
  if (ep0 == NULL) {
    return SYS_ERR_NO_MEM;
  }

  struct channel_endpoint* ep1 = kmem_cache_zalloc(&channel_cache);
  if (ep1 == NULL) {
    kmem_cache_free(&channel_cache, ep0);
    return SYS_ERR_NO_MEM;
  }

  spinlock_init(&ep0->lock);
  ep0->peer = ep1;
  ep0->signals = SIGNAL_WRITABLE;
  ep0->message_count = 0;
  ep0->msg_head = NULL;
  ep0->msg_tail = NULL;
  ep0->waiters_head = NULL;

  spinlock_init(&ep1->lock);
  ep1->peer = ep0;
  ep1->signals = SIGNAL_WRITABLE;
  ep1->message_count = 0;
  ep1->msg_head = NULL;
  ep1->msg_tail = NULL;
  ep1->waiters_head = NULL;

  *out_ep0 = ep0;
  *out_ep1 = ep1;
  return SYS_OK;
}

int channel_write(struct channel_endpoint* ep, const void* user_data,
                  size_t data_len, struct capability* transferred_caps,
                  size_t num_caps) {
  if (ep == NULL) {
    return SYS_ERR_INVALID_ARG;
  }
  if (data_len > CHANNEL_MAX_MESSAGE_BYTES) {
    return SYS_ERR_INVALID_ARG;
  }
  if (num_caps > CHANNEL_MAX_MESSAGE_CAPS) {
    return SYS_ERR_INVALID_ARG;
  }
  if (data_len > 0 && user_data == NULL) {
    return SYS_ERR_INVALID_ARG;
  }
  if (num_caps > 0 && transferred_caps == NULL) {
    return SYS_ERR_INVALID_ARG;
  }

  struct channel_message* msg = kmem_cache_zalloc(&message_cache);
  if (msg == NULL) {
    return SYS_ERR_NO_MEM;
  }

  if (data_len > 0) {
    if (data_len <= CHANNEL_INLINE_MESSAGE_BYTES) {
      msg->data = (uint8_t*)(msg + 1);
      msg->is_external = false;
    } else {
      uintptr_t phys = 0;
      if (pmm_alloc(1, &phys) != PMM_OK) {
        kmem_cache_free(&message_cache, msg);
        return SYS_ERR_NO_MEM;
      }
      msg->data = (uint8_t*)arch_phys_to_virt(phys);
      msg->is_external = true;
    }
    memory_copy(msg->data, user_data, data_len);
    msg->num_bytes = data_len;
  }

  if (num_caps > 0) {
    msg->num_caps = num_caps;
    for (size_t i = 0; i < num_caps; i++) {
      msg->caps[i] = transferred_caps[i];
    }
  }

  bool irq_ep = false;
  bool irq_peer = false;
  struct channel_endpoint* peer = NULL;
  lock_channel_pair(ep, &peer, &irq_ep, &irq_peer);

  if (peer == NULL) {
    unlock_channel_pair(ep, NULL, irq_ep, irq_peer);
    free_channel_message(msg);
    return SYS_ERR_PEER_CLOSED;
  }

  if (peer->message_count >= CHANNEL_MAX_QUEUE_DEPTH) {
    unlock_channel_pair(ep, peer, irq_ep, irq_peer);
    free_channel_message(msg);
    return SYS_ERR_WOULD_BLOCK;
  }

  msg->next = NULL;
  if (peer->msg_tail != NULL) {
    peer->msg_tail->next = msg;
    peer->msg_tail = msg;
  } else {
    peer->msg_head = msg;
    peer->msg_tail = msg;
  }
  peer->message_count++;
  peer->signals |= SIGNAL_READABLE;

  wake_waiters_locked(peer, SIGNAL_READABLE);

  unlock_channel_pair(ep, peer, irq_ep, irq_peer);
  return SYS_OK;
}

int channel_read(struct channel_endpoint* ep, void* user_buf,
                 size_t max_buf_len, size_t* out_actual_len,
                 struct capability* out_caps, size_t max_caps,
                 size_t* out_actual_caps) {
  if (ep == NULL) {
    return SYS_ERR_INVALID_ARG;
  }

  bool irq = spin_lock_irqsave(&ep->lock);

  if (ep->msg_head == NULL) {
    bool peer_closed = (ep->peer == NULL);
    spin_unlock_irqrestore(&ep->lock, irq);
    if (peer_closed) {
      return SYS_ERR_PEER_CLOSED;
    }
    return SYS_ERR_WOULD_BLOCK;
  }

  struct channel_message* msg = ep->msg_head;

  if (msg->num_bytes > max_buf_len || msg->num_caps > max_caps) {
    if (out_actual_len != NULL) {
      *out_actual_len = msg->num_bytes;
    }
    if (out_actual_caps != NULL) {
      *out_actual_caps = msg->num_caps;
    }
    spin_unlock_irqrestore(&ep->lock, irq);
    return SYS_ERR_BUFFER_TOO_SMALL;
  }

  ep->msg_head = msg->next;
  if (ep->msg_head == NULL) {
    ep->msg_tail = NULL;
    ep->signals &= ~SIGNAL_READABLE;
  }
  ep->message_count--;

  spin_unlock_irqrestore(&ep->lock, irq);

  if (msg->num_bytes > 0 && user_buf != NULL && msg->data != NULL) {
    memory_copy(user_buf, msg->data, msg->num_bytes);
  }
  if (out_actual_len != NULL) {
    *out_actual_len = msg->num_bytes;
  }

  if (msg->num_caps > 0 && out_caps != NULL) {
    for (size_t i = 0; i < msg->num_caps; i++) {
      out_caps[i] = msg->caps[i];
    }
  }
  if (out_actual_caps != NULL) {
    *out_actual_caps = msg->num_caps;
  }

  if (msg->is_external && msg->data != NULL) {
    uintptr_t phys = arch_virt_to_phys((virt_addr_t)msg->data);
    pmm_free((phys_addr_t)phys, 1);
    msg->data = NULL;
  }
  kmem_cache_free(&message_cache, msg);

  return SYS_OK;
}

void channel_endpoint_close(struct channel_endpoint* ep) {
  if (ep == NULL) {
    return;
  }

  bool irq_ep = false;
  bool irq_peer = false;
  struct channel_endpoint* peer = NULL;
  lock_channel_pair(ep, &peer, &irq_ep, &irq_peer);

  if (peer != NULL) {
    peer->peer = NULL;
    peer->signals |= SIGNAL_PEER_CLOSED;
    peer->signals &= ~SIGNAL_WRITABLE;
    wake_waiters_locked(peer, SIGNAL_PEER_CLOSED);
  }
  ep->peer = NULL;

  struct channel_message* msg = ep->msg_head;
  ep->msg_head = NULL;
  ep->msg_tail = NULL;
  ep->message_count = 0;
  ep->signals = SIGNAL_PEER_CLOSED;

  wake_waiters_locked(ep, SIGNAL_PEER_CLOSED);

  unlock_channel_pair(ep, peer, irq_ep, irq_peer);

  while (msg != NULL) {
    struct channel_message* next = msg->next;
    free_channel_message(msg);
    msg = next;
  }

  kmem_cache_free(&channel_cache, ep);
}

int channel_wait_one(struct channel_endpoint* ep, uint32_t interested_signals,
                     uint64_t timeout_ns, uint32_t* out_observed_signals) {
  if (ep == NULL) {
    return SYS_ERR_INVALID_ARG;
  }

  bool irq = spin_lock_irqsave(&ep->lock);

  uint32_t active = ep->signals & (interested_signals | SIGNAL_PEER_CLOSED);
  if (active != 0) {
    if (out_observed_signals != NULL) {
      *out_observed_signals = ep->signals;
    }
    spin_unlock_irqrestore(&ep->lock, irq);
    return SYS_OK;
  }

  if (timeout_ns == 0) {
    if (out_observed_signals != NULL) {
      *out_observed_signals = ep->signals;
    }
    spin_unlock_irqrestore(&ep->lock, irq);
    return SYS_ERR_TIMED_OUT;
  }

  struct task* current = sched_current_task();
  if (current == NULL) {
    spin_unlock_irqrestore(&ep->lock, irq);
    return SYS_ERR_INVALID_ARG;
  }

  struct channel_waiter* waiter = kmem_cache_zalloc(&waiter_cache);
  if (waiter == NULL) {
    spin_unlock_irqrestore(&ep->lock, irq);
    return SYS_ERR_NO_MEM;
  }

  waiter->task = current;
  waiter->interested_signals = interested_signals | SIGNAL_PEER_CLOSED;
  waiter->next = ep->waiters_head;
  ep->waiters_head = waiter;

  spin_unlock_irqrestore(&ep->lock, irq);

  sched_block_current(TASK_STATE_BLOCKED_IPC);

  return SYS_OK;
}
