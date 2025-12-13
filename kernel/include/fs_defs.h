#ifndef FS_DEFS_H
#define FS_DEFS_H

#include "util.h"

enum filetype {
    FT_FILE,
    FT_DIRECTORY,
};
#define NDIRECT 23
#define FILENAME_MAX_LEN 28
struct file_inode_data {
    uint32_t size;
    uint32_t blocks[NDIRECT];
};
struct directory_inode_data {
    uint32_t num_entries;
    uint32_t direct_files[NDIRECT];
    /* TODO: add indirect block
    uint32_t direct_files[NDIRECT - 1];
    uint32_t indirect_block;
    */
};
union inode_data {
    struct file_inode_data file_data;
    struct directory_inode_data directory_data;
};
struct inode {
    char name[FILENAME_MAX_LEN];
    enum filetype type;
    union inode_data data;
};
_Static_assert(sizeof(struct inode) == 128, "Bad inode size");

struct fs_dirent {
    // char name[FILENAME_MAX_LEN];
    uint32_t inode;
};
_Static_assert(sizeof(struct fs_dirent) == 4, "Bad dirent size");

#define NUM_INODE_BLOCKS 8

#define BLOCK_SIZE 4096
#define NUM_BLOCKS 300

#endif /* FS_DEFS_H */
