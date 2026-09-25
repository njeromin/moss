#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <common/syscall.h>
#include <sync/spinlock.h>
#include <proc/capabilities.h>

#define CHANNEL_MAX_MESSAGE_BYTES 4096
#define CHANNEL_MAX_MESSAGE_CAPS 8
#define CHANNEL_MAX_QUEUE_DEPTH 64
#define CHANNEL_INLINE_MESSAGE_BYTES 256

struct task;

struct channel_message {
  struct channel_message* next;
  size_t num_bytes;
  size_t num_caps;
  struct capability caps[CHANNEL_MAX_MESSAGE_CAPS];
  bool is_external;
  uint8_t* data;
};

struct channel_waiter {
  struct task* task;
  uint32_t interested_signals;
  struct channel_waiter* next;
};

struct channel_endpoint {
  struct spinlock lock;
  struct channel_endpoint* peer;
  uint32_t signals;
  size_t message_count;
  struct channel_message* msg_head;
  struct channel_message* msg_tail;
  struct channel_waiter* waiters_head;
};

// Subsystem initialisation
int init_channels(void);

// Create an interconnected endpoint pair
int channel_create_pair(struct channel_endpoint** out_ep0,
                        struct channel_endpoint** out_ep1);

// Write message and transfer capabilities to peer
int channel_write(struct channel_endpoint* ep, const void* user_data,
                  size_t data_len, struct capability* transferred_caps,
                  size_t num_caps);

// Read message and retrieve transferred capabilities
int channel_read(struct channel_endpoint* ep, void* user_buf,
                 size_t max_buf_len, size_t* out_actual_len,
                 struct capability* out_caps, size_t max_caps,
                 size_t* out_actual_caps);

// Destroy or close an endpoint (signals peer with SIGNAL_PEER_CLOSED)
void channel_endpoint_close(struct channel_endpoint* ep);

// Wait for signals on an endpoint
int channel_wait_one(struct channel_endpoint* ep, uint32_t interested_signals,
                     uint64_t timeout_ns, uint32_t* out_observed_signals);
