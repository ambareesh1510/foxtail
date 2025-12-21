#include "fs_defs.h"
#include "fs.h"
#include "kprintf.h"
#include "kstring.h"

char fs[NUM_BLOCKS * BLOCK_SIZE] = {
#embed "fs.bin"
};

struct inode *get_inode_at_idx(uint32_t idx) {
    return (struct inode *) (fs + idx * sizeof(struct inode));
}

struct inode *get_inode_by_path(
    struct inode *base,
    const char *path
) {
    if (base == 0) {
        return base;
    }
    if (path[0] == '/') {
        path++;
    }
    if (strlen(path) == 0) {
        return base;
    }
    if (base->type == FT_FILE) {
        return 0;
    }
    char path_buf[FILENAME_MAX_LEN] = {0};
    uint32_t i = 0;
    for (; path[i] != '/' && path[i] != '\0'; i++) {
        path_buf[i] = path[i];
    }
    path_buf[i] = '\0';
    struct inode *next = 0;
    for (uint32_t j = 0; j < base->data.directory_data.num_entries; j++) {
        struct inode *temp = get_inode_at_idx(base->data.directory_data.direct_files[j]);
        if (strcmp(temp->name, path_buf) == 0) {
            next = temp;
            break;
        }
    }
    return get_inode_by_path(next, path + i);
}

struct inode *get_root_inode() {
    return get_inode_at_idx(0);
}

void tree_helper(struct inode *base, uint32_t depth) {
    if (base == 0) {
        kprintf("Invalid inode\n");
        return;
    }
    if (depth > 0) {
        for (uint32_t i = 0; i < depth - 1; i++) {
            kprintf("   ");
        }
        kprintf("|- ");
    }
    if (base->type == FT_FILE) {
        kprintf("%s\n", base->name);
    } else {
        kprintf("%s/\n", base->name);
        for (uint32_t j = 0; j < base->data.directory_data.num_entries; j++) {
            struct inode *next = get_inode_at_idx(base->data.directory_data.direct_files[j]);
            tree_helper(next, depth + 1);
        }
    }
}

void tree(struct inode *base) {
    tree_helper(base, 0);
}

/// Get the block address that holds byte `offset` of `inode`.
///
/// `inode` must be a file.
uint32_t get_block_from_inode_offset(
    struct inode *inode,
    uint32_t offset
) {
    uint32_t linear_block = offset / BLOCK_SIZE;
    if (linear_block < NDIRECT) {
        return inode->data.file_data.blocks[linear_block];
    }
    // TODO: extend this when adding indirect blocks
    return 0;
}


uint32_t fs_read_bytes(struct inode *inode, uint32_t offset, uint32_t size, char *buf) {
    uint32_t total_bytes_read = 0;
    uint32_t buf_offset = 0;
    // Read bytes from the first block
    uint32_t first_block_read_size = min(
        size,
        BLOCK_SIZE - (offset % BLOCK_SIZE)
    );
    first_block_read_size = min(
        first_block_read_size, 
        inode->data.file_data.size - offset
    );
    uint32_t first_block_index = get_block_from_inode_offset(inode, offset);
    memcpy(buf,  fs + first_block_index * BLOCK_SIZE + offset % BLOCK_SIZE, first_block_read_size);
    size -= first_block_read_size;
    offset += first_block_read_size;
    buf_offset += first_block_read_size;
    total_bytes_read += first_block_read_size;

    // Read bytes from each remaining block
    for (;;) {
        uint32_t block_read_size = min(size, BLOCK_SIZE);
        block_read_size = min(
            block_read_size,
            inode->data.file_data.size - offset
        );
        uint32_t block_index = get_block_from_inode_offset(inode, offset);
        memcpy(
            buf + buf_offset,
            fs + block_index * BLOCK_SIZE,
            block_read_size
        );
        offset += block_read_size;
        buf_offset += block_read_size;
        total_bytes_read += block_read_size;
        if (size < BLOCK_SIZE) {
            break;
        }
        size -= BLOCK_SIZE;
    }
    return total_bytes_read;
}

void ls_entry(struct inode *entry) {
    if (entry->type == FT_FILE) {
        kprintf(
            "F (%d bytes) %s\n",
            entry->data.file_data.size,
            entry->name
        );
    } else {
        kprintf(
            "D (%d entries) %s\n",
            entry->data.directory_data.num_entries,
            entry->name
        );
    }
}

void ls(struct inode *base) {
    for (uint32_t j = 0; j < base->data.directory_data.num_entries; j++) {
        struct inode *next = get_inode_at_idx(base->data.directory_data.direct_files[j]);
        ls_entry(next);
    }
}
