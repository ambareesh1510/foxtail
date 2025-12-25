#ifndef FS_H
#define FS_H

#include "fs_defs.h"

extern char fs[NUM_BLOCKS * BLOCK_SIZE];

struct inode *alloc_inode();
void free_inode(struct inode *inode);

void acquire_inode(struct inode *inode);
void release_inode(struct inode *inode);

struct inode *get_inode_at_idx(u32 idx);

u32 get_index_from_inode(struct inode *inode);

struct inode *get_inode_by_path(
    struct inode *base,
    const char *path
);

struct inode *get_root_inode();

void tree(struct inode *base);
void ls(struct inode *base);

u32 fs_read_bytes(struct inode *inode, u32 offset, u32 size, char *buf);
u32 fs_write_bytes(struct inode *inode, u32 offset, u32 size, char *buf);

#endif /* FS_H */
