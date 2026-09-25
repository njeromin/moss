#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FSD_PROTOCOL_VERSION 1
#define FSD_MAX_PATH_LEN 256
#define FSD_MAX_NAME_LEN 64
#define FSD_MAX_PAYLOAD_LEN 3584
#define FSD_MAX_DIRENTS_PER_RESP 16

// Protocol opcodes
enum fsd_opcode {
  FSD_OP_OPEN = 1,
  FSD_OP_CLOSE = 2,
  FSD_OP_READ = 3,
  FSD_OP_WRITE = 4,
  FSD_OP_STAT = 5,
  FSD_OP_READDIR = 6,
  FSD_OP_CREATE = 7,
  FSD_OP_UNLINK = 8,
  FSD_OP_TRUNCATE = 9,
};

// Node classifications
enum fsd_node_type {
  FSD_NODE_FILE = 1,
  FSD_NODE_DIR = 2,
};

// Open and create flags
enum fsd_open_flags {
  FSD_OPEN_READ = (1u << 0),
  FSD_OPEN_WRITE = (1u << 1),
  FSD_OPEN_CREATE = (1u << 2),
  FSD_OPEN_TRUNCATE = (1u << 3),
  FSD_OPEN_DIRECTORY = (1u << 4),
};

// Protocol status and error codes
enum fsd_status {
  FSD_SUCCESS = 0,
  FSD_ERR_INVALID_ARG = -1,
  FSD_ERR_NOT_FOUND = -2,
  FSD_ERR_ALREADY_EXISTS = -3,
  FSD_ERR_NOT_DIR = -4,
  FSD_ERR_IS_DIR = -5,
  FSD_ERR_NO_SPACE = -6,
  FSD_ERR_ACCESS_DENIED = -7,
  FSD_ERR_IO = -8,
  FSD_ERR_NOT_SUPPORTED = -9,
  FSD_ERR_NAME_TOO_LONG = -10,
  FSD_ERR_NO_CONNECTION = -11,
};

// Common message framing header
struct fsd_msg_header {
  uint32_t txid;
  uint16_t version;
  uint16_t opcode;
  int32_t status;
  uint32_t payload_len;
};

// Node metadata attributes
struct fsd_attr {
  uint64_t inode;
  uint64_t size;
  uint32_t type;  // enum fsd_node_type
  uint32_t permissions;
};

// Directory entry format
struct fsd_dirent {
  uint64_t inode;
  uint32_t type;  // enum fsd_node_type
  uint32_t name_len;
  char name[FSD_MAX_NAME_LEN];
};

// Operation payload structures

// FSD_OP_OPEN
// Note: When issuing FSD_OP_OPEN, the client passes the new channel endpoint
// in transferred capability slot 0.
struct fsd_open_req {
  uint32_t flags;
  uint32_t path_len;
  char path[FSD_MAX_PATH_LEN];
};

struct fsd_open_resp {
  struct fsd_attr attr;
};

// FSD_OP_READ
struct fsd_read_req {
  uint64_t offset;
  uint32_t count;
};

struct fsd_read_resp {
  uint32_t actual_bytes;
  uint8_t data[FSD_MAX_PAYLOAD_LEN];
};

// FSD_OP_WRITE
struct fsd_write_req {
  uint64_t offset;
  uint32_t count;
  uint8_t data[FSD_MAX_PAYLOAD_LEN];
};

struct fsd_write_resp {
  uint32_t actual_bytes;
};

// FSD_OP_STAT
struct fsd_stat_req {
  uint32_t reserved;
};

struct fsd_stat_resp {
  struct fsd_attr attr;
};

// FSD_OP_READDIR
struct fsd_readdir_req {
  uint64_t cookie;
  uint32_t max_entries;
};

struct fsd_readdir_resp {
  uint64_t next_cookie;
  uint32_t count;
  struct fsd_dirent entries[FSD_MAX_DIRENTS_PER_RESP];
};

// FSD_OP_CREATE
struct fsd_create_req {
  uint32_t type;
  uint32_t name_len;
  char name[FSD_MAX_NAME_LEN];
};

struct fsd_create_resp {
  struct fsd_attr attr;
};

// FSD_OP_UNLINK
struct fsd_unlink_req {
  uint32_t name_len;
  char name[FSD_MAX_NAME_LEN];
};

struct fsd_unlink_resp {
  uint32_t reserved;
};

// FSD_OP_TRUNCATE
struct fsd_truncate_req {
  uint64_t size;
};

struct fsd_truncate_resp {
  uint64_t new_size;
};
