#pragma once

#include <stddef.h>
#include <stdint.h>
#include <fsd/protocol.h>

// Open a file or directory relative to a directory channel.
// Returns SYS_OK (0) on success and sets out_channel to the newly established endpoint.
// out_attr is optional and may be NULL.
int fsd_client_open(uint32_t dir_channel, const char* path, uint32_t flags,
                    uint32_t* out_channel, struct fsd_attr* out_attr);

// Read data from a file channel.
// out_actual receives the number of bytes read.
int fsd_client_read(uint32_t file_channel, uint64_t offset, void* buf,
                    size_t len, size_t* out_actual);

// Write data to a file channel.
// out_actual receives the number of bytes written.
int fsd_client_write(uint32_t file_channel, uint64_t offset, const void* buf,
                     size_t len, size_t* out_actual);

// Retrieve metadata attributes from a channel.
int fsd_client_stat(uint32_t channel, struct fsd_attr* out_attr);

// Read directory entries from a directory channel.
// inout_cookie tracks the iteration position.
// out_count receives the number of directory entries returned.
int fsd_client_readdir(uint32_t dir_channel, uint64_t* inout_cookie,
                       struct fsd_dirent* entries, size_t max_entries,
                       size_t* out_count);

// Create a file or directory inside a directory channel.
// If out_channel is non-NULL, establishes a new connection to the created node.
int fsd_client_create(uint32_t dir_channel, const char* name, uint32_t type,
                      uint32_t* out_channel, struct fsd_attr* out_attr);

// Unlink/remove an entry from a directory channel.
int fsd_client_unlink(uint32_t dir_channel, const char* name);

// Truncate/resize a file channel.
int fsd_client_truncate(uint32_t file_channel, uint64_t size);

// Close and release a client channel endpoint.
void fsd_client_close(uint32_t channel);
