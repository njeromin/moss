#pragma once

#include <stddef.h>
#include <stdint.h>

#define FDT_BEGIN_NODE 0x1 /* Start node: full name */
#define FDT_END_NODE 0x2   /* End node */
#define FDT_PROP 0x3       /* Property: name off, size, content */
#define FDT_NOP 0x4        /* nop */
#define FDT_END 0x9

// https://devicetree-specification.readthedocs.io/en/stable/flattened-format.html

struct fdt_header {
  uint32_t magic;
  uint32_t totalsize;
  uint32_t off_dt_struct;
  uint32_t off_dt_strings;
  uint32_t off_mem_rsvmap;
  uint32_t version;
  uint32_t last_comp_version;
  uint32_t boot_cpuid_phys;
  uint32_t size_dt_strings;
  uint32_t size_dt_struct;
};

struct fdt_node {
  uint32_t offset;
};

struct fdt_reserve_entry {
  uint64_t address;
  uint64_t size;
};

struct fdt_property {
  uint32_t len;
  uint32_t nameoff;
};

// Check the header and all block ranges described by the header.
int fdt_check_header(const void* dtb_blob);

// Check the complete tree. blob_size is the mapped/readable buffer bound.
int fdt_check_full(const void* dtb_blob, size_t blob_size);

int fdt_root(const void* blob, struct fdt_node* node);

int fdt_first_child(const void* blob, const struct fdt_node* parent,
                    struct fdt_node* child);

int fdt_next_sibling(const void* blob, const struct fdt_node* node,
                     struct fdt_node* sibling);

int fdt_node_name(const void *blob, const struct fdt_node *node,
                  const char **name, size_t *size);
int fdt_node_name_is(const void *blob, const struct fdt_node *node,
                     const char *name);

int fdt_get_property(const void* blob, const struct fdt_node* node,
                     const char* name, const void** data, size_t* size);

// Return a single NUL-terminated string property, excluding its terminator.
int fdt_property_string(const void *blob, const struct fdt_node *node,
                        const char *property, const char **value,
                        size_t *size);
int fdt_property_string_is(const void *blob, const struct fdt_node *node,
                           const char *property, const char *expected);

int fdt_property_u32(const void *blob, const struct fdt_node *node,
                     const char *name, uint32_t *value);
int fdt_property_u64(const void *blob, const struct fdt_node *node,
                     const char *name, uint64_t *value);
int fdt_property_i32(const void *blob, const struct fdt_node *node,
                     const char *name, int32_t *value);
int fdt_property_i64(const void *blob, const struct fdt_node *node,
                     const char *name, int64_t *value);
