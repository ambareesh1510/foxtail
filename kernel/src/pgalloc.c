#include "pgalloc.h"
#include "kprintf.h"


__attribute__((section(".boot.data")))
u32 page_free_map[PAGE_FREE_MAP_ENTRIES];

u32 *page_free_map_high = (u32 *) ((char *) page_free_map + HIGHER_HALF_BASE);
u32 page_alloc_ptr = (UPPER_MEM_START / PGSIZE) / 32;

u32 total_alloced_pages = 0;
/// Allocates and returns the index of a page
u32 alloc_page() {
    while (page_free_map_high[page_alloc_ptr] == 0) {
        page_alloc_ptr = (page_alloc_ptr + 1) % PAGE_FREE_MAP_ENTRIES;
    }
    for (u32 i = 0; i < 32; i++) {
        if (page_free_map_high[page_alloc_ptr] & (1 << i)) {
            page_free_map_high[page_alloc_ptr] &= ~(1 << i);
            total_alloced_pages++;
            // kprintf("alloced page %x\n", page_alloc_ptr * 32 + i);
            // kprintf("alloced a total of %d pages\n", total_alloced_pages);
            return page_alloc_ptr * 32 + i;
        }
    }
    // TODO: should return a proper error value
    panic("can't alloc page\n");
    return 0;
}

/// Frees the page at the given index.
/// Returns true on success, false on failure.
u32 total_freed_pages = 0;
bool free_page(u32 page_index) {
    if (page_free_map_high[page_index / 32] & (1 << (page_index % 32))) {
        return false;
    } else {
        page_free_map_high[page_index / 32] |= (1 << (page_index % 32));
        total_freed_pages++;
        // kprintf("freed a total of %d\n", total_freed_pages);
        return true;
    }
}
