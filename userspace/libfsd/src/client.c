#include <fsd/client.h>

#include <string.h>
#include <syscall.h>
#include <common/memory.h>
#include <common/strings.h>
#include <common/syscall.h>
#include "helpers.h"

static int fsd_client_transact(uint32_t channel, uint16_t opcode,
                               const void* req_payload, size_t req_payload_len,
                               const uint32_t* send_caps, size_t num_send_caps,
                               void* resp_payload, size_t max_resp_len,
                               size_t* out_resp_len) {
  uint8_t tx_buf[CHANNEL_MAX_MESSAGE_BYTES];
  if (sizeof(struct fsd_msg_header) + req_payload_len > sizeof(tx_buf)) {
    return FSD_ERR_INVALID_ARG;
  }

  struct fsd_msg_header* tx_hdr = (struct fsd_msg_header*)tx_buf;
  tx_hdr->txid = 1;
  tx_hdr->version = FSD_PROTOCOL_VERSION;
  tx_hdr->opcode = opcode;
  tx_hdr->status = FSD_SUCCESS;
  tx_hdr->payload_len = (uint32_t)req_payload_len;

  if (req_payload != NULL && req_payload_len > 0) {
    memory_copy(tx_buf + sizeof(struct fsd_msg_header), req_payload,
                req_payload_len);
  }

  size_t total_tx = sizeof(struct fsd_msg_header) + req_payload_len;
  int err =
      sys_channel_write(channel, tx_buf, total_tx, send_caps, num_send_caps);
  if (err != SYS_OK) {
    return FSD_ERR_IO;
  }

  uint32_t observed = 0;
  err = sys_object_wait_one(channel, SIGNAL_READABLE | SIGNAL_PEER_CLOSED,
                            1000000000ULL, &observed);
  if (err != SYS_OK || (observed & SIGNAL_PEER_CLOSED)) {
    return FSD_ERR_IO;
  }

  uint8_t rx_buf[CHANNEL_MAX_MESSAGE_BYTES];
  size_t actual_rx_bytes = 0;
  size_t actual_rx_caps = 0;
  err = sys_channel_read(channel, rx_buf, sizeof(rx_buf), &actual_rx_bytes,
                         NULL, 0, &actual_rx_caps);
  if (err != SYS_OK || actual_rx_bytes < sizeof(struct fsd_msg_header)) {
    return FSD_ERR_IO;
  }

  const struct fsd_msg_header* rx_hdr = (const struct fsd_msg_header*)rx_buf;
  if (rx_hdr->status != FSD_SUCCESS) {
    return rx_hdr->status;
  }

  size_t rx_payload_len = actual_rx_bytes - sizeof(struct fsd_msg_header);
  if (resp_payload != NULL && max_resp_len > 0) {
    size_t copy_len =
        (rx_payload_len < max_resp_len) ? rx_payload_len : max_resp_len;
    memory_copy(resp_payload, rx_buf + sizeof(struct fsd_msg_header), copy_len);
  }

  if (out_resp_len != NULL) {
    *out_resp_len = rx_payload_len;
  }

  return FSD_SUCCESS;
}

int fsd_client_open(uint32_t dir_channel, const char* path, uint32_t flags,
                    uint32_t* out_channel, struct fsd_attr* out_attr) {
  if (dir_channel == 0 || path == NULL || out_channel == NULL) {
    return FSD_ERR_INVALID_ARG;
  }

  uint32_t client_ep = 0;
  uint32_t server_ep = 0;
  int err =
      sys_channel_create(0, CAP_SLOT_ANY, CAP_SLOT_ANY, &client_ep, &server_ep);
  if (err != SYS_OK) {
    return FSD_ERR_NO_SPACE;
  }

  struct fsd_open_req req;
  memory_set(&req, 0, sizeof(req));
  req.flags = flags;
  req.path_len = (uint32_t)strlcpy(req.path, path, sizeof(req.path));

  uint32_t send_caps[1] = {server_ep};
  struct fsd_open_resp resp;
  memory_set(&resp, 0, sizeof(resp));

  err = fsd_client_transact(dir_channel, FSD_OP_OPEN, &req, sizeof(req),
                            send_caps, 1, &resp, sizeof(resp), NULL);
  if (err != FSD_SUCCESS) {
    sys_cap_delete(client_ep);
    return err;
  }

  *out_channel = client_ep;
  if (out_attr != NULL) {
    *out_attr = resp.attr;
  }
  return FSD_SUCCESS;
}

int fsd_client_read(uint32_t file_channel, uint64_t offset, void* buf,
                    size_t len, size_t* out_actual) {
  if (file_channel == 0 || buf == NULL) {
    return FSD_ERR_INVALID_ARG;
  }

  struct fsd_read_req req;
  req.offset = offset;
  req.count = (len > FSD_MAX_PAYLOAD_LEN) ? (uint32_t)FSD_MAX_PAYLOAD_LEN
                                          : (uint32_t)len;

  struct fsd_read_resp resp;
  memory_set(&resp, 0, sizeof(resp));

  int err = fsd_client_transact(file_channel, FSD_OP_READ, &req, sizeof(req),
                                NULL, 0, &resp, sizeof(resp), NULL);
  if (err != FSD_SUCCESS) {
    return err;
  }

  size_t copy_bytes = (resp.actual_bytes < len) ? resp.actual_bytes : len;
  memory_copy(buf, resp.data, copy_bytes);

  if (out_actual != NULL) {
    *out_actual = copy_bytes;
  }
  return FSD_SUCCESS;
}

int fsd_client_write(uint32_t file_channel, uint64_t offset, const void* buf,
                     size_t len, size_t* out_actual) {
  if (file_channel == 0 || buf == NULL) {
    return FSD_ERR_INVALID_ARG;
  }

  struct fsd_write_req req;
  req.offset = offset;
  size_t to_write = (len > FSD_MAX_PAYLOAD_LEN) ? FSD_MAX_PAYLOAD_LEN : len;
  req.count = (uint32_t)to_write;
  memory_copy(req.data, buf, to_write);

  struct fsd_write_resp resp;
  memory_set(&resp, 0, sizeof(resp));

  size_t req_size = sizeof(uint64_t) + sizeof(uint32_t) + to_write;
  int err = fsd_client_transact(file_channel, FSD_OP_WRITE, &req, req_size,
                                NULL, 0, &resp, sizeof(resp), NULL);
  if (err != FSD_SUCCESS) {
    return err;
  }

  if (out_actual != NULL) {
    *out_actual = resp.actual_bytes;
  }
  return FSD_SUCCESS;
}

int fsd_client_stat(uint32_t channel, struct fsd_attr* out_attr) {
  if (channel == 0 || out_attr == NULL) {
    return FSD_ERR_INVALID_ARG;
  }

  struct fsd_stat_req req;
  req.reserved = 0;

  struct fsd_stat_resp resp;
  memory_set(&resp, 0, sizeof(resp));

  int err = fsd_client_transact(channel, FSD_OP_STAT, &req, sizeof(req), NULL,
                                0, &resp, sizeof(resp), NULL);
  if (err != FSD_SUCCESS) {
    return err;
  }

  *out_attr = resp.attr;
  return FSD_SUCCESS;
}

int fsd_client_readdir(uint32_t dir_channel, uint64_t* inout_cookie,
                       struct fsd_dirent* entries, size_t max_entries,
                       size_t* out_count) {
  if (dir_channel == 0 || inout_cookie == NULL || entries == NULL ||
      max_entries == 0) {
    return FSD_ERR_INVALID_ARG;
  }

  struct fsd_readdir_req req;
  req.cookie = *inout_cookie;
  req.max_entries = (max_entries > FSD_MAX_DIRENTS_PER_RESP)
                        ? FSD_MAX_DIRENTS_PER_RESP
                        : (uint32_t)max_entries;

  struct fsd_readdir_resp resp;
  memory_set(&resp, 0, sizeof(resp));

  int err = fsd_client_transact(dir_channel, FSD_OP_READDIR, &req, sizeof(req),
                                NULL, 0, &resp, sizeof(resp), NULL);
  if (err != FSD_SUCCESS) {
    return err;
  }

  size_t copy_count = (resp.count < max_entries) ? resp.count : max_entries;
  for (size_t i = 0; i < copy_count; i++) {
    entries[i] = resp.entries[i];
  }

  *inout_cookie = resp.next_cookie;
  if (out_count != NULL) {
    *out_count = copy_count;
  }
  return FSD_SUCCESS;
}

int fsd_client_create(uint32_t dir_channel, const char* name, uint32_t type,
                      uint32_t* out_channel, struct fsd_attr* out_attr) {
  if (dir_channel == 0 || name == NULL) {
    return FSD_ERR_INVALID_ARG;
  }

  uint32_t client_ep = 0;
  uint32_t server_ep = 0;
  uint32_t send_caps[1] = {0};
  size_t num_caps = 0;

  if (out_channel != NULL) {
    int err = sys_channel_create(0, CAP_SLOT_ANY, CAP_SLOT_ANY, &client_ep,
                                 &server_ep);
    if (err != SYS_OK) {
      return FSD_ERR_NO_SPACE;
    }
    send_caps[0] = server_ep;
    num_caps = 1;
  }

  struct fsd_create_req req;
  memory_set(&req, 0, sizeof(req));
  req.type = type;
  req.name_len = (uint32_t)strlcpy(req.name, name, sizeof(req.name));

  struct fsd_create_resp resp;
  memory_set(&resp, 0, sizeof(resp));

  int err = fsd_client_transact(dir_channel, FSD_OP_CREATE, &req, sizeof(req),
                                send_caps, num_caps, &resp, sizeof(resp), NULL);
  if (err != FSD_SUCCESS) {
    if (out_channel != NULL) {
      sys_cap_delete(client_ep);
    }
    return err;
  }

  if (out_channel != NULL) {
    *out_channel = client_ep;
  }
  if (out_attr != NULL) {
    *out_attr = resp.attr;
  }
  return FSD_SUCCESS;
}

int fsd_client_unlink(uint32_t dir_channel, const char* name) {
  if (dir_channel == 0 || name == NULL) {
    return FSD_ERR_INVALID_ARG;
  }

  struct fsd_unlink_req req;
  memory_set(&req, 0, sizeof(req));
  req.name_len = (uint32_t)strlcpy(req.name, name, sizeof(req.name));

  struct fsd_unlink_resp resp;
  return fsd_client_transact(dir_channel, FSD_OP_UNLINK, &req, sizeof(req),
                             NULL, 0, &resp, sizeof(resp), NULL);
}

int fsd_client_truncate(uint32_t file_channel, uint64_t size) {
  if (file_channel == 0) {
    return FSD_ERR_INVALID_ARG;
  }

  struct fsd_truncate_req req;
  req.size = size;

  struct fsd_truncate_resp resp;
  return fsd_client_transact(file_channel, FSD_OP_TRUNCATE, &req, sizeof(req),
                             NULL, 0, &resp, sizeof(resp), NULL);
}

void fsd_client_close(uint32_t channel) {
  if (channel != 0) {
    sys_cap_delete(channel);
  }
}
