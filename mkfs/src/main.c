#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "fs_defs.h"

char *fs_dir;
char *fs_out;

const char *get_filename(const char *path) {
    const char *filename = strrchr(path, '/');
    return filename ? filename + 1 : path;
}

size_t process_file(
    struct inode *inodes,
    size_t *curr_inode,
    char *data_blocks,
    size_t *curr_data_block,
    char *bitmap,
    char *name
) {
    FILE *f;
    f = fopen(name, "rb");
    if (f == NULL) {
        printf("[ERR] Could not open file %s\n", name);
        return 0;
    }
    printf("[LOG] Adding file %s\n", name);
    struct inode new_inode = {0};
    strncpy(new_inode.name, get_filename(name), FILENAME_MAX_LEN - 1);
    new_inode.type = FT_FILE;
    new_inode.valid = 1;
    new_inode.num_refs = 0;
    char buf[BLOCK_SIZE] = {0};
    size_t inode_block_idx = 0;
    size_t total_size = 0;
    size_t read_size = 0;
    // TODO: Test if this while loop condition actually works
    while ((read_size = fread(buf, 1, BLOCK_SIZE, f)) != 0) {
        if (inode_block_idx >= NDIRECT) {
            break;
        }
        memcpy(
            data_blocks + (*curr_data_block - NUM_INODE_BLOCKS - NUM_BITMAP_BLOCKS) * BLOCK_SIZE,
            buf,
            BLOCK_SIZE
        );
        memset(buf, 0, BLOCK_SIZE);
        new_inode.data.file_data.blocks[inode_block_idx] = *curr_data_block;
        inode_block_idx++;
        // Mark block as used
        bitmap[(*curr_data_block) / 8] |= 1 << ((*curr_data_block) % 8);
        (*curr_data_block)++;
        total_size += read_size;
    }
    new_inode.data.file_data.size = total_size;
    inodes[*curr_inode] = new_inode;
    fclose(f);
    return (*curr_inode)++;
}

int process_dir(
    struct inode *inodes,
    size_t *curr_inode,
    char *data_blocks,
    size_t *curr_data_block,
    char *bitmap,
    char *name
) {
    DIR *d;
    struct dirent *dir;
    d = opendir(name);
    if (d == NULL) {
        printf("[ERR] Could not open directory %s\n", name);
        // TODO: (return 0 ==> error) isn't actually true because root returns inode 0...
        // but we can ignore that for now since we don't use the return value when this function is called on root
        return 0;
    }
    size_t reserved_inode_idx = *curr_inode;
    (*curr_inode)++;
    struct inode new_inode = {0};
    if (strcmp(name, fs_dir) == 0) {
        strcpy(new_inode.name, FS_ROOT_PATH);
        new_inode.parent = 0;
    } else {
        strncpy(new_inode.name, get_filename(name), FILENAME_MAX_LEN - 1);
    }
    printf("[LOG] Adding dir %s\n", new_inode.name);
    new_inode.type = FT_DIRECTORY;
    new_inode.valid = 1;
    new_inode.num_refs = 0;
    new_inode.data.directory_data.num_entries = 0;
    size_t dirent_idx = 0;
    // TODO: write blocks for dir
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0 || strcmp(dir->d_name, name) == 0) {
            continue;
        }
        int inode_idx = 0;
        size_t len = strlen(name) + strlen(dir->d_name) + 2;
        char *buf = malloc(len);
        strcpy(buf, name);
        strcat(buf, "/");
        strcat(buf, dir->d_name);
        if (dir->d_type == DT_REG) {
            inode_idx = process_file(inodes, curr_inode, data_blocks, curr_data_block, bitmap, buf);
        } else if (dir->d_type == DT_DIR) {
            inode_idx = process_dir(inodes, curr_inode, data_blocks, curr_data_block, bitmap, buf);
        }
        free(buf);
        if (inode_idx == 0) {
            continue;
        }
        inodes[inode_idx].parent = reserved_inode_idx;
        new_inode.data.directory_data.direct_files[new_inode.data.directory_data.num_entries] = inode_idx;
        new_inode.data.directory_data.num_entries++;
    }
    closedir(d);
    inodes[reserved_inode_idx] = new_inode;
    return reserved_inode_idx;
}

int main(int argc, char **argv) {
#if CHAR_BIT != 8
#error CHAR_BIT != 8
#endif

    if (argc != 3) {
        printf("[ERR] Incorrect number of arguments\n");
        printf("[LOG] Usage: mkfs [fs_dir] [fs_out_file]");
        return 1;
    }
    fs_dir = argv[1];
    fs_out = argv[2];

    char bitmap[NUM_BITMAP_BLOCKS * BLOCK_SIZE] = {0};
    for (int i = 0; i < NUM_BITMAP_BLOCKS + NUM_INODE_BLOCKS; i++) {
        bitmap[i / 8] |= 1 << (i % 8);
    }
    struct inode inodes[NUM_INODE_BLOCKS * (BLOCK_SIZE / sizeof(struct inode))] = {0};
    size_t curr_inode = 0;
    char data_blocks[BLOCK_SIZE * (NUM_BLOCKS - NUM_INODE_BLOCKS)] = {0};
    size_t curr_data_block = NUM_BITMAP_BLOCKS + NUM_INODE_BLOCKS;
    // size_t curr_data_block = NUM_INODE_BLOCKS;

    process_dir(inodes, &curr_inode, data_blocks, &curr_data_block, bitmap, fs_dir);

    FILE *fs;
    fs = fopen(fs_out, "wb");
    if (fs == NULL) {
        printf("[ERR] Failed to open %s for writing\n", fs_out);
        return 1;
    }
    printf("[LOG] Writing filesystem to disk at %s\n", fs_out);
    fwrite(bitmap, BLOCK_SIZE, NUM_BITMAP_BLOCKS, fs);
    fwrite(inodes, sizeof(struct inode), sizeof(inodes) / sizeof(struct inode), fs);
    fwrite(data_blocks, BLOCK_SIZE, NUM_BLOCKS - NUM_INODE_BLOCKS - NUM_BITMAP_BLOCKS, fs);
    fclose(fs);

    return 0;
}
