#include <fsd/server.h>

#include <string.h>
#include <syscall.h>
#include <common/memory.h>
#include <common/strings.h>
#include <common/syscall.h>
#include "helpers.h"

static int fsd_server_send_reply(uint32_t channel_slot, uint32_t txid,
                                 uint16_t opcode, int32_t status,
                                 const void* payload, size_t payload_len) {
  uint8_t send_buf[CHANNEL_MAX_MESSAGE_BYTES];
  if (sizeof(struct fsd_msg_header) + payload_len > sizeof(send_buf)) {
    return FSD_ERR_INVALID_ARG;
  }

  struct fsd_msg_header* hdr = (struct fsd_msg_header*)send_buf;
  hdr->txid = txid;
  hdr->version = FSD_PROTOCOL_VERSION;
  hdr->opcode = opcode;
  hdr->status = status;
  hdr->payload_len = (uint32_t)payload_len;

  if (payload != NULL && payload_len > 0) {
    memory_copy(send_buf + sizeof(struct fsd_msg_header), payload, payload_len);
  }

  size_t total_len = sizeof(struct fsd_msg_header) + payload_len;
  int err = sys_channel_write(channel_slot, send_buf, total_len, NULL, 0);
  if (err != SYS_OK) {
    return FSD_ERR_IO;
  }
  return FSD_SUCCESS;
}

int fsd_server_init(struct fsd_server* server, const struct fsd_ops* ops,
                    void* fs_ctx, uint32_t root_channel_slot, void* root_node) {
  if (server == NULL || ops == NULL) {
    return FSD_ERR_INVALID_ARG;
  }

  server->ops = ops;
  server->fs_ctx = fs_ctx;
  server->active_connections = 0;
  server->should_terminate = false;

  for (size_t i = 0; i < FSD_SERVER_MAX_CONNECTIONS; i++) {
    server->connections[i].channel_slot = 0;
    server->connections[i].node = NULL;
    server->connections[i].is_dir = false;
    server->connections[i].active = false;
  }

  if (root_channel_slot != 0 && root_node != NULL) {
    return fsd_server_add_connection(server, root_channel_slot, root_node,
                                     true);
  }

  return FSD_SUCCESS;
}

int fsd_server_add_connection(struct fsd_server* server, uint32_t channel_slot,
                              void* node, bool is_dir) {
  if (server == NULL || channel_slot == 0 || node == NULL) {
    return FSD_ERR_INVALID_ARG;
  }

  for (size_t i = 0; i < FSD_SERVER_MAX_CONNECTIONS; i++) {
    if (!server->connections[i].active) {
      server->connections[i].channel_slot = channel_slot;
      server->connections[i].node = node;
      server->connections[i].is_dir = is_dir;
      server->connections[i].active = true;
      server->active_connections++;
      return FSD_SUCCESS;
    }
  }

  return FSD_ERR_NO_SPACE;
}

void fsd_server_close_connection(struct fsd_server* server, size_t index) {
  if (server == NULL || index >= FSD_SERVER_MAX_CONNECTIONS) {
    return;
  }

  if (!server->connections[index].active) {
    return;
  }

  if (server->ops != NULL && server->ops->release_node != NULL &&
      server->connections[index].node != NULL) {
    server->ops->release_node(server->fs_ctx, server->connections[index].node);
  }

  if (server->connections[index].channel_slot != 0) {
    sys_cap_delete(server->connections[index].channel_slot);
  }

  server->connections[index].channel_slot = 0;
  server->connections[index].node = NULL;
  server->connections[index].is_dir = false;
  server->connections[index].active = false;

  if (server->active_connections > 0) {
    server->active_connections--;
  }
}

static void fsd_server_handle_open(struct fsd_server* server, size_t conn_idx,
                                   const struct fsd_msg_header* hdr,
                                   const uint8_t* payload, size_t payload_len,
                                   const uint32_t* recv_caps, size_t num_caps) {
  struct fsd_connection* conn = &server->connections[conn_idx];
  uint32_t client_cap = (num_caps > 0) ? recv_caps[0] : 0;

  if (payload_len < sizeof(struct fsd_open_req)) {
    if (client_cap != 0) {
      sys_cap_delete(client_cap);
    }
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_INVALID_ARG, NULL, 0);
    return;
  }

  const struct fsd_open_req* req = (const struct fsd_open_req*)payload;
  void* curr_node = conn->node;
  bool curr_is_dir = conn->is_dir;

  char comp[FSD_MAX_NAME_LEN];
  const char* path_ptr = req->path;
  const char* remaining = NULL;
  bool found_any = false;

  int status = FSD_SUCCESS;

  while (fsd_path_next_component(path_ptr, comp, sizeof(comp), &remaining)) {
    found_any = true;

    if (!curr_is_dir) {
      status = FSD_ERR_NOT_DIR;
      break;
    }

    void* next_node = NULL;
    bool next_is_dir = false;

    if (server->ops->lookup == NULL) {
      status = FSD_ERR_NOT_SUPPORTED;
      break;
    }

    int err = server->ops->lookup(server->fs_ctx, curr_node, comp, &next_node,
                                  &next_is_dir);

    // If not found and this is the last component and create flag was given
    if (err == FSD_ERR_NOT_FOUND && remaining == NULL &&
        (req->flags & FSD_OPEN_CREATE)) {
      if (server->ops->create == NULL) {
        status = FSD_ERR_NOT_SUPPORTED;
        break;
      }
      uint32_t create_type =
          (req->flags & FSD_OPEN_DIRECTORY) ? FSD_NODE_DIR : FSD_NODE_FILE;
      err = server->ops->create(server->fs_ctx, curr_node, comp, create_type,
                                &next_node);
      if (err == FSD_SUCCESS) {
        next_is_dir = (create_type == FSD_NODE_DIR);
      }
    }

    if (err != FSD_SUCCESS) {
      status = err;
      break;
    }

    curr_node = next_node;
    curr_is_dir = next_is_dir;
    path_ptr = remaining;
    if (remaining == NULL) {
      break;
    }
  }

  if (status == FSD_SUCCESS && !found_any) {
    // Empty path or "/" requested - refers to the current directory node
    curr_node = conn->node;
    curr_is_dir = conn->is_dir;
  }

  if (status == FSD_SUCCESS) {
    if ((req->flags & FSD_OPEN_DIRECTORY) && !curr_is_dir) {
      status = FSD_ERR_NOT_DIR;
    }
  }

  if (status != FSD_SUCCESS) {
    if (client_cap != 0) {
      sys_cap_delete(client_cap);
    }
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode, status,
                          NULL, 0);
    return;
  }

  // Obtain node attributes
  struct fsd_open_resp resp;
  memory_set(&resp, 0, sizeof(resp));
  if (server->ops->getattr != NULL) {
    server->ops->getattr(server->fs_ctx, curr_node, &resp.attr);
  }

  // If client provided a capability endpoint, bind it to the discovered node
  if (client_cap != 0) {
    int bind_err =
        fsd_server_add_connection(server, client_cap, curr_node, curr_is_dir);
    if (bind_err != FSD_SUCCESS) {
      sys_cap_delete(client_cap);
      fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                            bind_err, NULL, 0);
      return;
    }
  }

  fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode, FSD_SUCCESS,
                        &resp, sizeof(resp));
}

static void fsd_server_handle_read(struct fsd_server* server, size_t conn_idx,
                                   const struct fsd_msg_header* hdr,
                                   const uint8_t* payload, size_t payload_len) {
  struct fsd_connection* conn = &server->connections[conn_idx];
  if (conn->is_dir) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_IS_DIR, NULL, 0);
    return;
  }

  if (payload_len < sizeof(struct fsd_read_req)) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_INVALID_ARG, NULL, 0);
    return;
  }

  const struct fsd_read_req* req = (const struct fsd_read_req*)payload;
  if (server->ops->read == NULL) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_NOT_SUPPORTED, NULL, 0);
    return;
  }

  struct fsd_read_resp resp;
  memory_set(&resp, 0, sizeof(resp));
  size_t to_read = req->count;
  if (to_read > FSD_MAX_PAYLOAD_LEN) {
    to_read = FSD_MAX_PAYLOAD_LEN;
  }

  size_t actual_read = 0;
  int err = server->ops->read(server->fs_ctx, conn->node, req->offset,
                              resp.data, to_read, &actual_read);
  if (err != FSD_SUCCESS) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode, err, NULL,
                          0);
    return;
  }

  resp.actual_bytes = (uint32_t)actual_read;
  size_t reply_payload_size = sizeof(uint32_t) + actual_read;
  fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode, FSD_SUCCESS,
                        &resp, reply_payload_size);
}

static void fsd_server_handle_write(struct fsd_server* server, size_t conn_idx,
                                    const struct fsd_msg_header* hdr,
                                    const uint8_t* payload,
                                    size_t payload_len) {
  struct fsd_connection* conn = &server->connections[conn_idx];
  if (conn->is_dir) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_IS_DIR, NULL, 0);
    return;
  }

  if (payload_len < sizeof(struct fsd_write_req)) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_INVALID_ARG, NULL, 0);
    return;
  }

  const struct fsd_write_req* req = (const struct fsd_write_req*)payload;
  if (server->ops->write == NULL) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_NOT_SUPPORTED, NULL, 0);
    return;
  }

  size_t to_write = req->count;
  if (to_write > FSD_MAX_PAYLOAD_LEN) {
    to_write = FSD_MAX_PAYLOAD_LEN;
  }

  size_t actual_written = 0;
  int err = server->ops->write(server->fs_ctx, conn->node, req->offset,
                               req->data, to_write, &actual_written);
  if (err != FSD_SUCCESS) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode, err, NULL,
                          0);
    return;
  }

  struct fsd_write_resp resp;
  resp.actual_bytes = (uint32_t)actual_written;
  fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode, FSD_SUCCESS,
                        &resp, sizeof(resp));
}

static void fsd_server_handle_stat(struct fsd_server* server, size_t conn_idx,
                                   const struct fsd_msg_header* hdr) {
  struct fsd_connection* conn = &server->connections[conn_idx];
  if (server->ops->getattr == NULL) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_NOT_SUPPORTED, NULL, 0);
    return;
  }

  struct fsd_stat_resp resp;
  memory_set(&resp, 0, sizeof(resp));
  int err = server->ops->getattr(server->fs_ctx, conn->node, &resp.attr);
  if (err != FSD_SUCCESS) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode, err, NULL,
                          0);
    return;
  }

  fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode, FSD_SUCCESS,
                        &resp, sizeof(resp));
}

static void fsd_server_handle_readdir(struct fsd_server* server,
                                      size_t conn_idx,
                                      const struct fsd_msg_header* hdr,
                                      const uint8_t* payload,
                                      size_t payload_len) {
  struct fsd_connection* conn = &server->connections[conn_idx];
  if (!conn->is_dir) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_NOT_DIR, NULL, 0);
    return;
  }

  if (payload_len < sizeof(struct fsd_readdir_req)) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_INVALID_ARG, NULL, 0);
    return;
  }

  const struct fsd_readdir_req* req = (const struct fsd_readdir_req*)payload;
  if (server->ops->readdir == NULL) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_NOT_SUPPORTED, NULL, 0);
    return;
  }

  struct fsd_readdir_resp resp;
  memory_set(&resp, 0, sizeof(resp));
  uint64_t cookie = req->cookie;
  size_t count = 0;

  size_t max_req = req->max_entries;
  if (max_req == 0 || max_req > FSD_MAX_DIRENTS_PER_RESP) {
    max_req = FSD_MAX_DIRENTS_PER_RESP;
  }

  int err = server->ops->readdir(server->fs_ctx, conn->node, &cookie,
                                 resp.entries, max_req, &count);
  if (err != FSD_SUCCESS) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode, err, NULL,
                          0);
    return;
  }

  resp.next_cookie = cookie;
  resp.count = (uint32_t)count;

  size_t payload_size =
      sizeof(uint64_t) + sizeof(uint32_t) + (count * sizeof(struct fsd_dirent));
  fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode, FSD_SUCCESS,
                        &resp, payload_size);
}

static void fsd_server_handle_create(struct fsd_server* server, size_t conn_idx,
                                     const struct fsd_msg_header* hdr,
                                     const uint8_t* payload, size_t payload_len,
                                     const uint32_t* recv_caps,
                                     size_t num_caps) {
  struct fsd_connection* conn = &server->connections[conn_idx];
  uint32_t client_cap = (num_caps > 0) ? recv_caps[0] : 0;

  if (!conn->is_dir) {
    if (client_cap != 0) {
      sys_cap_delete(client_cap);
    }
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_NOT_DIR, NULL, 0);
    return;
  }

  if (payload_len < sizeof(struct fsd_create_req)) {
    if (client_cap != 0) {
      sys_cap_delete(client_cap);
    }
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_INVALID_ARG, NULL, 0);
    return;
  }

  const struct fsd_create_req* req = (const struct fsd_create_req*)payload;
  if (server->ops->create == NULL) {
    if (client_cap != 0) {
      sys_cap_delete(client_cap);
    }
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_NOT_SUPPORTED, NULL, 0);
    return;
  }

  void* new_node = NULL;
  int err = server->ops->create(server->fs_ctx, conn->node, req->name,
                                req->type, &new_node);
  if (err != FSD_SUCCESS) {
    if (client_cap != 0) {
      sys_cap_delete(client_cap);
    }
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode, err, NULL,
                          0);
    return;
  }

  struct fsd_create_resp resp;
  memory_set(&resp, 0, sizeof(resp));
  if (server->ops->getattr != NULL) {
    server->ops->getattr(server->fs_ctx, new_node, &resp.attr);
  }

  if (client_cap != 0) {
    int bind_err = fsd_server_add_connection(server, client_cap, new_node,
                                             req->type == FSD_NODE_DIR);
    if (bind_err != FSD_SUCCESS) {
      sys_cap_delete(client_cap);
      fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                            bind_err, NULL, 0);
      return;
    }
  }

  fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode, FSD_SUCCESS,
                        &resp, sizeof(resp));
}

static void fsd_server_handle_unlink(struct fsd_server* server, size_t conn_idx,
                                     const struct fsd_msg_header* hdr,
                                     const uint8_t* payload,
                                     size_t payload_len) {
  struct fsd_connection* conn = &server->connections[conn_idx];
  if (!conn->is_dir) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_NOT_DIR, NULL, 0);
    return;
  }

  if (payload_len < sizeof(struct fsd_unlink_req)) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_INVALID_ARG, NULL, 0);
    return;
  }

  const struct fsd_unlink_req* req = (const struct fsd_unlink_req*)payload;
  if (server->ops->unlink == NULL) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_NOT_SUPPORTED, NULL, 0);
    return;
  }

  int err = server->ops->unlink(server->fs_ctx, conn->node, req->name);
  if (err != FSD_SUCCESS) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode, err, NULL,
                          0);
    return;
  }

  struct fsd_unlink_resp resp;
  resp.reserved = 0;
  fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode, FSD_SUCCESS,
                        &resp, sizeof(resp));
}

static void fsd_server_handle_truncate(struct fsd_server* server,
                                       size_t conn_idx,
                                       const struct fsd_msg_header* hdr,
                                       const uint8_t* payload,
                                       size_t payload_len) {
  struct fsd_connection* conn = &server->connections[conn_idx];
  if (conn->is_dir) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_IS_DIR, NULL, 0);
    return;
  }

  if (payload_len < sizeof(struct fsd_truncate_req)) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_INVALID_ARG, NULL, 0);
    return;
  }

  const struct fsd_truncate_req* req = (const struct fsd_truncate_req*)payload;
  if (server->ops->truncate == NULL) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                          FSD_ERR_NOT_SUPPORTED, NULL, 0);
    return;
  }

  int err = server->ops->truncate(server->fs_ctx, conn->node, req->size);
  if (err != FSD_SUCCESS) {
    fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode, err, NULL,
                          0);
    return;
  }

  struct fsd_truncate_resp resp;
  resp.new_size = req->size;
  fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode, FSD_SUCCESS,
                        &resp, sizeof(resp));
}

static void fsd_server_dispatch_message(struct fsd_server* server,
                                        size_t conn_idx) {
  struct fsd_connection* conn = &server->connections[conn_idx];
  uint8_t in_buf[CHANNEL_MAX_MESSAGE_BYTES];
  uint32_t recv_caps[CHANNEL_MAX_MESSAGE_CAPS];
  size_t actual_bytes = 0;
  size_t actual_caps = 0;

  int err = sys_channel_read(conn->channel_slot, in_buf, sizeof(in_buf),
                             &actual_bytes, recv_caps, CHANNEL_MAX_MESSAGE_CAPS,
                             &actual_caps);
  if (err != SYS_OK || actual_bytes < sizeof(struct fsd_msg_header)) {
    for (size_t i = 0; i < actual_caps; i++) {
      if (recv_caps[i] != 0) {
        sys_cap_delete(recv_caps[i]);
      }
    }
    return;
  }

  const struct fsd_msg_header* hdr = (const struct fsd_msg_header*)in_buf;
  const uint8_t* payload = in_buf + sizeof(struct fsd_msg_header);
  size_t payload_len = actual_bytes - sizeof(struct fsd_msg_header);

  switch (hdr->opcode) {
    case FSD_OP_OPEN:
      {
        fsd_server_handle_open(server, conn_idx, hdr, payload, payload_len,
                               recv_caps, actual_caps);
        break;
      }
    case FSD_OP_READ:
      {
        fsd_server_handle_read(server, conn_idx, hdr, payload, payload_len);
        break;
      }
    case FSD_OP_WRITE:
      {
        fsd_server_handle_write(server, conn_idx, hdr, payload, payload_len);
        break;
      }
    case FSD_OP_STAT:
      {
        fsd_server_handle_stat(server, conn_idx, hdr);
        break;
      }
    case FSD_OP_READDIR:
      {
        fsd_server_handle_readdir(server, conn_idx, hdr, payload, payload_len);
        break;
      }
    case FSD_OP_CREATE:
      {
        fsd_server_handle_create(server, conn_idx, hdr, payload, payload_len,
                                 recv_caps, actual_caps);
        break;
      }
    case FSD_OP_UNLINK:
      {
        fsd_server_handle_unlink(server, conn_idx, hdr, payload, payload_len);
        break;
      }
    case FSD_OP_TRUNCATE:
      {
        fsd_server_handle_truncate(server, conn_idx, hdr, payload, payload_len);
        break;
      }
    case FSD_OP_CLOSE:
      {
        fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                              FSD_SUCCESS, NULL, 0);
        fsd_server_close_connection(server, conn_idx);
        break;
      }
    default:
      {
        for (size_t i = 0; i < actual_caps; i++) {
          if (recv_caps[i] != 0) {
            sys_cap_delete(recv_caps[i]);
          }
        }
        fsd_server_send_reply(conn->channel_slot, hdr->txid, hdr->opcode,
                              FSD_ERR_NOT_SUPPORTED, NULL, 0);
        break;
      }
  }
}

int fsd_server_step(struct fsd_server* server, uint64_t timeout_ns) {
  if (server == NULL || server->active_connections == 0) {
    return FSD_ERR_NO_CONNECTION;
  }

  bool processed_any = false;

  for (size_t i = 0; i < FSD_SERVER_MAX_CONNECTIONS; i++) {
    if (!server->connections[i].active) {
      continue;
    }

    uint32_t observed = 0;
    int err =
        sys_object_wait_one(server->connections[i].channel_slot,
                            SIGNAL_READABLE | SIGNAL_PEER_CLOSED, 0, &observed);
    if (err == SYS_OK) {
      if (observed & SIGNAL_PEER_CLOSED) {
        fsd_server_close_connection(server, i);
        processed_any = true;
      } else if (observed & SIGNAL_READABLE) {
        fsd_server_dispatch_message(server, i);
        processed_any = true;
      }
    }
  }

  if (processed_any || timeout_ns == 0) {
    return FSD_SUCCESS;
  }

  // If only a single connection is registered and timeout_ns > 0, wait on it directly
  if (server->active_connections == 1) {
    for (size_t i = 0; i < FSD_SERVER_MAX_CONNECTIONS; i++) {
      if (server->connections[i].active) {
        uint32_t observed = 0;
        int err = sys_object_wait_one(server->connections[i].channel_slot,
                                      SIGNAL_READABLE | SIGNAL_PEER_CLOSED,
                                      timeout_ns, &observed);
        if (err == SYS_OK) {
          if (observed & SIGNAL_PEER_CLOSED) {
            fsd_server_close_connection(server, i);
          } else if (observed & SIGNAL_READABLE) {
            fsd_server_dispatch_message(server, i);
          }
        }
        return (err == SYS_OK) ? FSD_SUCCESS : FSD_ERR_IO;
      }
    }
  } else {
    // With multiple connections and no event ready, yield execution to avoid busy-spinning
    sys_yield();
  }

  return FSD_SUCCESS;
}

int fsd_server_run(struct fsd_server* server) {
  if (server == NULL) {
    return FSD_ERR_INVALID_ARG;
  }

  while (!server->should_terminate && server->active_connections > 0) {
    fsd_server_step(server, 1000000000ULL);
  }

  return FSD_SUCCESS;
}
