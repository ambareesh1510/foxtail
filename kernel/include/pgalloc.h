#ifndef PGALLOC_H
#define PGALLOC_H

#include "paging.h"

#define UPPER_MEM_START 0x100000
#define MEM_MAX 0x100000000

#define PAGE_FREE_MAP_ENTRIES ((MEM_MAX / PGSIZE) / 32)

__attribute__((section(".boot.data")))
extern uint32_t page_free_map[PAGE_FREE_MAP_ENTRIES];
extern uint32_t *page_free_map_high;

[[nodiscard]]
uint32_t alloc_page();

bool free_page(uint32_t page_index);

#endif /* PGALLOC_H */
