#ifndef FS_H
#define FS_H

#include "fs_defs.h"

extern char fs[NUM_BLOCKS * BLOCK_SIZE];

struct inode *get_inode_at_idx(uint32_t idx);


struct inode *get_inode_by_path(
    struct inode *base,
    const char *path
);

struct inode *get_root_inode();

void tree(struct inode *base, uint32_t depth);

uint32_t fs_read_bytes(struct inode *inode, uint32_t offset, uint32_t size, char *buf);

#endif /* FS_H */
