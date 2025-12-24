#ifndef MALLOC_H
#define MALLOC_H

#include "syscall_defs.h"

struct block {
    unsigned int size;
    struct block *next;
    bool free;
};

#define BLOCK_SIZE sizeof(struct block)
#define ALIGN 8
#define align_up(x) (((x) + ALIGN - 1) & ~(ALIGN - 1))

struct block *free_list = 0;

struct block *find_free_block(unsigned int size) {
    struct block *curr = free_list;
    while (curr) {
        if (curr->free && curr->size >= size) {
            return curr;
        }
        curr = curr->next;
    }
    return 0;
}

struct block *request_mem(unsigned int size) {
    struct block *block = (struct block *) sbrk(BLOCK_SIZE + size);
    
    block->size = size;
    block->next = 0;
    block->free = 0;
    
    return block;
}

void *malloc(unsigned int size) {
    if (size == 0) {
        return 0;
    }
    
    size = align_up(size);
    
    struct block *block;
    if (free_list == 0) {
        block = request_mem(size);
        free_list = block;
    } else {
        block = find_free_block(size);
        if (block) {
            block->free = 0;
        } else {
            block = request_mem(size);
            // TODO: consider resizing block to only what's needed
            struct block *curr = free_list;
            while (curr->next) {
                curr = curr->next;
            }
            curr->next = block;
        }
    }
    
    return (void *)(block + 1);
}

void free(void *ptr) {
    if (ptr == 0) {
        return;
    }
    
    struct block *block = ((struct block *) ptr) - 1;
    block->free = 1;
    
    // TODO: Merge adjacent free blocks
}

void *calloc(unsigned int nmemb, unsigned int size) {
    unsigned int total = nmemb * size;
    void *ptr = malloc(total);
    
    if (ptr) {
        char *p = (char *) ptr;
        for (unsigned int i = 0; i < total; i++) {
            p[i] = 0;
        }
    }
    
    return ptr;
}

void *realloc(void *ptr, unsigned int size) {
    if (ptr == 0) {
        return malloc(size);
    }
    
    if (size == 0) {
        free(ptr);
        return 0;
    }
    
    struct block *block = ((struct block *) ptr) - 1;
    
    // If current block is big enough, return
    if (block->size >= size) {
        return ptr;
    }
    
    void *new_ptr = malloc(size);
    
    // Copy data
    char *src = (char *)ptr;
    char *dst = (char *)new_ptr;
    for (unsigned int i = 0; i < block->size && i < size; i++) {
        dst[i] = src[i];
    }
    
    free(ptr);
    return new_ptr;
}

#endif /* MALLOC_H */
