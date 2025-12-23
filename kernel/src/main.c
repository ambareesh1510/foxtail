#include "exec.h"
#include "interrupt.h"
#include "gdt.h"
#include "proc.h"
#include "vga.h"
#include "paging.h"
#include "fs.h"
#include "kprintf.h"
#include "multiboot_defs.h"
#include "pgalloc.h"
#include "util.h"
#include "kstring.h"

struct higher_half_info {
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t mmap_len;
    uint32_t mmap_addr;
};

void higher_half_entry();

__attribute__ ((section(".boot.data")))
uint32_t free_pages = 0;

void 
__attribute__ ((section(".boot.text")))
kernel_main(void) {
    struct multiboot_info *multiboot_info;
    __asm__ volatile (
        "mov %%ebx, %0\n"
        : "=r" (multiboot_info)
    );
    if (multiboot_info->flags & (1 << 6)) {
        uint32_t total_len = 0;
        struct memory_map *entry = (struct memory_map *) multiboot_info->mmap_addr;
        while (total_len < multiboot_info->mmap_length) {
            total_len += entry->size + 4;
            if (entry->type == 1) {
                for (
                    uint32_t curr_addr = PAGE_ROUND_DOWN(entry->base_addr_low);
                    curr_addr < PAGE_ROUND_DOWN(entry->base_addr_low) + entry->length_low;
                    curr_addr += PGSIZE
                ) {
                    if (curr_addr < 0x400000) continue;
                    uint32_t entry = (curr_addr / PGSIZE) / 32;
                    uint32_t offset = (curr_addr / PGSIZE) % 32;
                    page_free_map[entry] |= 1 << offset;
                    free_pages++;
                }
            }
            entry = (struct memory_map *) (((char *) entry) + entry->size + 4);
        }
    } else if (multiboot_info->flags & (1 << 0)) {
        for (
            uint32_t curr_addr = UPPER_MEM_START;
            curr_addr < UPPER_MEM_START + multiboot_info->mem_upper;
            curr_addr += PGSIZE
        ) {
            uint32_t entry = (curr_addr / PGSIZE) / 32;
            uint32_t offset = (curr_addr / PGSIZE) % 32;
            page_free_map[entry] |= 1 << offset;
        }
    } else {
        // TODO: Panic!
    }
    
    paging_setup(kernel_pgdir, kernel_id_pgtbl);

    // Increment stack pointer and base pointer so they point to higher half
    // addresses. We need this so that we can use the stack once the lower half
    // mapping is invalidated.
    __asm__ volatile (
        "add $0xC0000000, %esp\n"
        "add $0xC0000000, %ebp\n"
    );


    // __asm__ volatile (
    //     "calll %0\n"
    //     : : "r"(higher_half_entry)
    // );
    __asm__ volatile(
        "jmp *%0\n"
        : : "r"((uint32_t) higher_half_entry)
    );
    // higher_half_entry();
//     void (*hh_entry)(void) =
//     (void (*)(void))((uint32_t)higher_half_entry + HIGHER_HALF_BASE);
//
// hh_entry();

}


char a_kernel_stack[2 * PGSIZE];
void
higher_half_entry() {
    __asm__ volatile (
        "mov %0, %%esp"
        : : "r"(a_kernel_stack + 2 * PGSIZE)
    );
    // Unmap the identity mapping of the lower half.
    kernel_pgdir[0] = 0;
    __asm__ volatile (
        "mov %cr3, %eax\n"
        "mov %eax, %cr3\n"
    );

    vga_clear();

    // We need to set the GDT to be able to use segment selectors in the higher
    // half.
    gdt_load();
    idt_load();

    // kprintf("total free pages: %x\n", free_pages);
    kprintf("Total free pages: %d\n", *(uint32_t *) ((char *) &free_pages + HIGHER_HALF_BASE));
    // panic("");
    //
    struct inode *idle_inode = get_inode_by_path(get_root_inode(), "idle");
    if (idle_inode == 0) {
        panic("Idle program not found");
    }
    exec_helper(idle_inode);

    struct inode *exe_inode = get_inode_by_path(get_root_inode(), "test");
    if (exe_inode == 0) {
        panic("Init program not found");
    }
    exec(exe_inode);
    // exec(idle_inode);

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
