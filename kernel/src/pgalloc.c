#include "pgalloc.h"


__attribute__((section(".boot.data")))
uint32_t page_free_map[PAGE_FREE_MAP_ENTRIES];

uint32_t *page_free_map_high = (uint32_t *) ((char *) page_free_map + HIGHER_HALF_BASE);
uint32_t page_alloc_ptr = (UPPER_MEM_START / PGSIZE) / 32;

/// Allocates and returns the index of a page
uint32_t alloc_page() {
    while (page_free_map_high[page_alloc_ptr] == 0) {
        page_alloc_ptr = (page_alloc_ptr + 1) % PAGE_FREE_MAP_ENTRIES;
    }
    for (uint32_t i = 0; i < 32; i++) {
        if (page_free_map_high[page_alloc_ptr] & (1 << i)) {
            page_free_map_high[page_alloc_ptr] &= ~(1 << i);
            return page_alloc_ptr * 32 + i;
        }
    }
    // TODO: should return a proper error value
    return 0;
}

/// Frees the page at the given index.
/// Returns true on success, false on failure.
bool free_page(uint32_t page_index) {
    if (page_free_map_high[page_index / 32] & (1 << (page_index % 32))) {
        return false;
    } else {
        page_free_map_high[page_index / 32] |= (1 << (page_index % 32));
        return true;
    }
}
