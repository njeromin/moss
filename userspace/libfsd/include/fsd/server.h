#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <fsd/protocol.h>

#define FSD_SERVER_MAX_CONNECTIONS 64

// Backend filesystem operations interface to be implemented by filesystem drivers (e.g. ext2d, ramfsd)
struct fsd_ops {
  // Look up a path component relative to parent_node.
  // out_node: set to opaque handle representing the found node.
  // out_is_dir: set to true if directory, false if regular file.
  int (*lookup)(void* fs_ctx, void* parent_node, const char* name,
                void** out_node, bool* out_is_dir);

  // Create a new child node (file or directory) under parent_node.
  int (*create)(void* fs_ctx, void* parent_node, const char* name,
                uint32_t type, void** out_node);

  // Read data from a regular file node.
  int (*read)(void* fs_ctx, void* node, uint64_t offset, void* buf, size_t len,
              size_t* out_actual);

  // Write data to a regular file node.
  int (*write)(void* fs_ctx, void* node, uint64_t offset, const void* buf,
               size_t len, size_t* out_actual);

  // Retrieve metadata attributes for a node.
  int (*getattr)(void* fs_ctx, void* node, struct fsd_attr* out_attr);

  // Read directory entries starting from inout_cookie.
  // out_count receives the number of entries populated into out_entries.
  int (*readdir)(void* fs_ctx, void* node, uint64_t* inout_cookie,
                 struct fsd_dirent* out_entries, size_t max_entries,
                 size_t* out_count);

  // Unlink/remove a child entry from a parent directory.
  int (*unlink)(void* fs_ctx, void* parent_node, const char* name);

  // Truncate/resize a file node.
  int (*truncate)(void* fs_ctx, void* node, uint64_t size);

  // Release/dereference a node when a client connection closes.
  void (*release_node)(void* fs_ctx, void* node);
};

// Internal connection representation
struct fsd_connection {
  uint32_t channel_slot;
  void* node;
  bool is_dir;
  bool active;
};

// Filesystem daemon server context
struct fsd_server {
  const struct fsd_ops* ops;
  void* fs_ctx;
  struct fsd_connection connections[FSD_SERVER_MAX_CONNECTIONS];
  size_t active_connections;
  bool should_terminate;
};

// Initialise the server with operations, context, and the initial root channel + root node
int fsd_server_init(struct fsd_server* server, const struct fsd_ops* ops,
                    void* fs_ctx, uint32_t root_channel_slot, void* root_node);

// Register a new connection endpoint bound to a specific node
int fsd_server_add_connection(struct fsd_server* server, uint32_t channel_slot,
                              void* node, bool is_dir);

// Close and release an active connection by its slot index
void fsd_server_close_connection(struct fsd_server* server, size_t index);

// Process pending events across active connections
// If timeout_ns == 0, executes in non-blocking polling mode.
// If timeout_ns > 0, waits for events.
int fsd_server_step(struct fsd_server* server, uint64_t timeout_ns);

// Run the server loop until all connections terminate or should_terminate is set
int fsd_server_run(struct fsd_server* server);
