#include "interrupt.h"
#include "gdt.h"
#include "vga.h"
#include "paging.h"
#include "fs.h"
#include "kprintf.h"
#include "multiboot_defs.h"
#include "pgalloc.h"
#include "util.h"
#include "kstring.h"

__attribute__((aligned(4096))) 
__attribute__ ((section(".boot.data")))
uint32_t kernel_pgdir[1024] = {0};
__attribute__((aligned(4096)))
__attribute__ ((section(".boot.data")))
uint32_t kernel_id_pgtbl[1024] = {0};

struct higher_half_info {
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t mmap_len;
    uint32_t mmap_addr;
};

void higher_half_entry();

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
                    uint32_t entry = (curr_addr / PGSIZE) / 32;
                    uint32_t offset = (curr_addr / PGSIZE) % 32;
                    page_free_map[entry] |= 1 << offset;
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

    higher_half_entry();
}


void
higher_half_entry() {
    // Unmap the identity mapping of the lower half.
    kernel_pgdir[0] = 0;
    __asm__ volatile ("invlpg [0]");

    vga_clear();

    // We need to set the GDT to be able to use segment selectors in the higher
    // half.
    gdt_load();
    idt_load();

    __asm__ volatile (
        "mov $0x12345678, %eax\n"
        "mov $0x85, %ebx\n"
        "mov $0x75, %ecx\n"
        "mov $0x95, %edx\n"
        "int $0x80"
    );

    kprintf("%x\n", page_free_map_high[(UPPER_MEM_START / PGSIZE) / 32]);
    uint32_t allocated = alloc_page();
    kprintf("allocated page %x\n", allocated);
    kprintf("%x\n", page_free_map_high[(UPPER_MEM_START / PGSIZE) / 32]);
    bool res = free_page(allocated);
    if (res) {
        kprintf("Successfully deallocated\n");
    } else {
        kprintf("Failed to deallocate\n");
    }

    tree(get_root_inode());

    char buf[10000] = {0};
    struct inode *cfg_inode = get_inode_by_path(get_root_inode(), "hi/test.txt");
    uint32_t read = fs_read_bytes(cfg_inode, 0, 9999, buf);
    // kprintf("%s\n", buf);
    kprintf("file size: %d\n", cfg_inode->data.file_data.size);
    kprintf("read bytes: %d\n", read);
    kprintf("strlen: %d\n", strlen(buf));
    ls(get_root_inode());

    // for (;;) {
    //     kprintf("Ticks: %d, kb: %x\n", ticks, (uint32_t) (unsigned char) kb_char);
    // }

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
