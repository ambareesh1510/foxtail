#include "fs_defs.h"
#include "fs.h"
#include "kprintf.h"
#include "kstring.h"

char fs[NUM_BLOCKS * BLOCK_SIZE] = {
#embed "fs.bin"
};

char *free_block_bitmap = fs;
struct inode *inodes = (struct inode *) (fs + NUM_BITMAP_BLOCKS * BLOCK_SIZE);

i32 alloc_block() {
    for (u32 i = 0; i < NUM_BLOCKS; i++) {
        if (free_block_bitmap[i / 8] & (1 << (i % 8))) {
            continue;
        }
        free_block_bitmap[i / 8] |= (1 << (i % 8));
        return i;
    }
    return -1;
}

bool free_block(u32 block_idx) {
    if (free_block_bitmap[block_idx / 8] & (1 << (block_idx % 8))) {
        return false;
    } else {
        free_block_bitmap[block_idx / 8] &= ~(1 << (block_idx % 8));
        return true;
    }
}

struct inode *alloc_inode() {
    for (u32 i = 0; i < (NUM_INODE_BLOCKS * BLOCK_SIZE) / sizeof(struct inode); i++) {
        if (inodes[i].type == FT_UNALLOCATED) {
            return inodes + i;
        }
    }
    return 0;
}

i32 free_inode(struct inode *inode) {
    if (inode->type == FT_FILE) {
        u32 num_blocks = (inode->data.file_data.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        for (u32 i = 0; i < num_blocks; i++) {
            free_block(inode->data.file_data.blocks[i]);
        }
    } else if (inode->type == FT_DIRECTORY) {
        if (inode->data.directory_data.num_entries != 0) {
            return -1;
        }
    }
    inode->type = FT_UNALLOCATED;
    return 0;
}

struct inode *get_inode_at_idx(u32 idx) {
    return (struct inode *) (fs + NUM_BITMAP_BLOCKS * BLOCK_SIZE + idx * sizeof(struct inode));
}

u32 get_index_from_inode(struct inode *inode) {
    return ((u32) inode - (u32) inodes) / sizeof(struct inode);
}

struct inode *get_inode_by_path(
    struct inode *base,
    const char *path
) {
    if (base == 0) {
        return 0;
    }
    while (path[0] == '/') {
        path++;
    }
    if (strlen(path) == 0) {
        return base;
    }
    if (base->type == FT_FILE) {
        return 0;
    }
    char path_buf[FILENAME_MAX_LEN] = {0};
    u32 i = 0;
    for (; path[i] != '/' && path[i] != '\0'; i++) {
        path_buf[i] = path[i];
    }
    path_buf[i] = '\0';
    struct inode *next = 0;
    if (strcmp(path_buf, ".") == 0) {
        next = base;
    } else if (strcmp(path_buf, "..") == 0) {
        next = get_inode_at_idx(base->parent);
    } else {
        for (u32 j = 0; j < base->data.directory_data.num_entries; j++) {
            struct inode *temp = get_inode_at_idx(base->data.directory_data.direct_files[j]);
            if (strcmp(temp->name, path_buf) == 0) {
                next = temp;
                break;
            }
        }
    }
    return get_inode_by_path(next, path + i);
}

struct inode *get_root_inode() {
    return get_inode_at_idx(0);
}

void tree_helper(struct inode *base, u32 depth) {
    if (base == 0) {
        kprintf("Invalid inode\n");
        return;
    }
    if (depth > 0) {
        for (u32 i = 0; i < depth - 1; i++) {
            kprintf("   ");
        }
        kprintf("|- ");
    }
    if (base->type == FT_FILE) {
        kprintf("%s\n", base->name);
    } else {
        kprintf("%s/\n", base->name);
        for (u32 j = 0; j < base->data.directory_data.num_entries; j++) {
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
u32 get_block_from_inode_offset(
    struct inode *inode,
    u32 offset
) {
    u32 linear_block = offset / BLOCK_SIZE;
    if (linear_block < NDIRECT) {
        return inode->data.file_data.blocks[linear_block];
    }
    // TODO: extend this when adding indirect blocks
    return 0;
}

#define BLOCK_ROUND_UP(x) (((x + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE)

u32 fs_move_bytes(struct inode *inode, u32 offset, u32 size, char *buf, bool is_read) {
    // If it's a write and the file isn't big enough, allocate more blocks
    // TODO: test this
    if (!is_read) {
        u32 end = offset + size;
        if (end > inode->data.file_data.size) {
            if (end > FILE_MAX_SIZE) {
                // The write would exceed max file size; truncate the size of the write accordingly
                size = inode->data.file_data.size - offset;
            }
            if (inode->data.file_data.size > FILE_MAX_SIZE - BLOCK_SIZE) {
                // All blocks are already allocated; continue.
            } else {
                u32 unallocated_block = (inode->data.file_data.size + BLOCK_SIZE - 1) / BLOCK_SIZE + 1;
                u32 final_block = (end + BLOCK_SIZE - 1) / BLOCK_SIZE;
                while (unallocated_block < final_block) {
                    inode->data.file_data.blocks[unallocated_block] = alloc_block();
                    unallocated_block++;
                }
                inode->data.file_data.size = end;
            }
        }
    }
    u32 total_bytes_moved = 0;
    u32 buf_offset = 0;
    // Move bytes from the first block
    u32 first_block_move_size = min(
        size,
        BLOCK_SIZE - (offset % BLOCK_SIZE)
    );
    first_block_move_size = min(
        first_block_move_size, 
        inode->data.file_data.size - offset
    );
    u32 first_block_index = get_block_from_inode_offset(inode, offset);
    if (is_read) {
        memcpy(buf,  fs + first_block_index * BLOCK_SIZE + offset % BLOCK_SIZE, first_block_move_size);
    } else {
        memcpy(fs + first_block_index * BLOCK_SIZE + offset % BLOCK_SIZE, buf, first_block_move_size);
    }
    size -= first_block_move_size;
    offset += first_block_move_size;
    buf_offset += first_block_move_size;
    total_bytes_moved += first_block_move_size;

    // Move bytes from each remaining block
    for (;;) {
        u32 block_move_size = min(size, BLOCK_SIZE);
        block_move_size = min(
            block_move_size,
            inode->data.file_data.size - offset
        );
        u32 block_index = get_block_from_inode_offset(inode, offset);
        if (is_read) {
            memcpy(
                buf + buf_offset,
                fs + block_index * BLOCK_SIZE,
                block_move_size
            );
        } else {
            memcpy(
                fs + block_index * BLOCK_SIZE,
                buf + buf_offset,
                block_move_size
            );
        }
        offset += block_move_size;
        buf_offset += block_move_size;
        total_bytes_moved += block_move_size;
        if (size < BLOCK_SIZE) {
            break;
        }
        size -= BLOCK_SIZE;
    }
    return total_bytes_moved;
}


u32 fs_read_bytes(struct inode *inode, u32 offset, u32 size, char *buf) {
    return fs_move_bytes(inode, offset, size, buf, true);
}

u32 fs_write_bytes(struct inode *inode, u32 offset, u32 size, char *buf) {
    return fs_move_bytes(inode, offset, size, buf, false);
}

void ls_entry(struct inode *entry) {
    if (entry->type == FT_FILE) {
        kprintf(
            "F (%d bytes) %s\n",
            entry->data.file_data.size,
            entry->name
        );
        for (u32 block = 0; block < entry->data.file_data.size / BLOCK_SIZE + 1; block++) {
            kprintf("  block %d\n", entry->data.file_data.blocks[block]);
        }
    } else {
        kprintf(
            "D (%d entries) %s\n",
            entry->data.directory_data.num_entries,
            entry->name
        );
    }
}

void ls(struct inode *base) {
    for (u32 j = 0; j < base->data.directory_data.num_entries; j++) {
        struct inode *next = get_inode_at_idx(base->data.directory_data.direct_files[j]);
        ls_entry(next);
    }
}
