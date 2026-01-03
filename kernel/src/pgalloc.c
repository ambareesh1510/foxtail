#include "pgalloc.h"
#include "kprintf.h"
#include "paging.h"


__attribute__((section(".boot.data")))
u32 page_free_map[PAGE_FREE_MAP_ENTRIES];

u32 *page_free_map_high = (u32 *) ((char *) page_free_map + HIGHER_HALF_BASE);
u32 page_alloc_ptr = (UPPER_MEM_START / PGSIZE) / 32;
u32 *page_alloc_ptr_low = (u32 *) ((u32) (&page_alloc_ptr) - HIGHER_HALF_BASE);

/// Allocates and returns the index of a page
u32 alloc_page() {
    while (page_free_map_high[page_alloc_ptr] == 0) {
        page_alloc_ptr = (page_alloc_ptr + 1) % PAGE_FREE_MAP_ENTRIES;
    }
    for (u32 i = 0; i < 32; i++) {
        if (page_free_map_high[page_alloc_ptr] & (1 << i)) {
            page_free_map_high[page_alloc_ptr] &= ~(1 << i);
            return page_alloc_ptr * 32 + i;
        }
    }
    // TODO: should return a proper error value
    panic("can't alloc page\n");
    return 0;
}

u32 alloc_page_low() {
    while (page_free_map[*page_alloc_ptr_low] == 0) {
        *page_alloc_ptr_low = (*page_alloc_ptr_low + 1) % PAGE_FREE_MAP_ENTRIES;
    }
    for (u32 i = 0; i < 32; i++) {
        if (page_free_map[*page_alloc_ptr_low] & (1 << i)) {
            page_free_map[*page_alloc_ptr_low] &= ~(1 << i);
            return (*page_alloc_ptr_low) * 32 + i;
        }
    }
    // TODO: should return a proper error value
    panic("can't alloc page\n");
    return 0;
}

/// Frees the page at the given index.
/// Returns true on success, false on failure.
bool free_page(u32 page_index) {
    if (page_free_map_high[page_index / 32] & (1 << (page_index % 32))) {
        return false;
    } else {
        page_free_map_high[page_index / 32] |= (1 << (page_index % 32));
        return true;
    }
}
