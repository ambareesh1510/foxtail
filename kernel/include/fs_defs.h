#ifndef FS_DEFS_H
#define FS_DEFS_H

#include "util.h"

#define FS_ROOT_PATH "~"

enum filetype {
    FT_UNALLOCATED,
    FT_FILE,
    FT_DIRECTORY,
    FT_SYMLINK,
};
#define NDIRECT 19
#define NINDIRECT 2
#define FILE_DIRECT_MAX_SIZE (NDIRECT * BLOCK_SIZE)
#define FILE_INDIRECT_MAX_SIZE (NINDIRECT * (BLOCK_SIZE / sizeof(u32)) * BLOCK_SIZE)
#define FILE_MAX_SIZE (FILE_DIRECT_MAX_SIZE + FILE_INDIRECT_MAX_SIZE)
#define PTRS_PER_INDIRECT (BLOCK_SIZE / sizeof(u32))
#define DIR_MAX_ENTRIES (NDIRECT + NINDIRECT)
#define FILENAME_MAX_LEN 28
// TODO: file truncate
struct file_inode_data {
    u32 size;
    u32 direct_blocks[NDIRECT];
    u32 indirect_blocks[NINDIRECT];
};
// TODO: indirect blocks for directories
struct directory_inode_data {
    u32 num_entries;
    u32 direct_files[NDIRECT + NINDIRECT];
    /* TODO: add indirect block
    uint32_t direct_files[NDIRECT - 1];
    uint32_t indirect_block;
    */
};
struct symlink_inode_data {
    u32 target;
};
union inode_data {
    struct file_inode_data file_data;
    struct directory_inode_data directory_data;
    struct symlink_inode_data symlink_data;
};
struct inode {
    char name[FILENAME_MAX_LEN];
    enum filetype type;
    u32 parent;
    union inode_data data;
    // valid = 0 if the file should be deleted
    u16 valid;
    u16 num_refs;
};
_Static_assert(sizeof(struct inode) == 128, "Bad inode size");

struct fs_dirent {
    // char name[FILENAME_MAX_LEN];
    u32 inode;
};
_Static_assert(sizeof(struct fs_dirent) == 4, "Bad dirent size");

#define NUM_INODE_BLOCKS 8

#define BLOCK_SIZE 4096
#define NUM_BLOCKS 2048

#if NUM_BLOCKS % 8 != 0
#error NUM_BLOCKS % 8 != 0
#endif

#define NUM_BITMAP_BYTES (NUM_BLOCKS / 8)
#define NUM_BITMAP_BLOCKS ((NUM_BITMAP_BYTES + BLOCK_SIZE - 1) / BLOCK_SIZE)

#endif /* FS_DEFS_H */
