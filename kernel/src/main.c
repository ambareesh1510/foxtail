#include "interrupt.h"
#include "gdt.h"
#include "vga.h"
#include "paging.h"
#include "fs.h"
#include "kprintf.h"
#include "util.h"
#include "kstring.h"

__attribute__((aligned(4096))) 
__attribute__ ((section(".boot.data")))
uint32_t kernel_pgdir[1024] = {0};
__attribute__((aligned(4096)))
__attribute__ ((section(".boot.data")))
uint32_t kernel_id_pgtbl[1024] = {0};

void higher_half_entry();

void 
__attribute__ ((section(".boot.text")))
kernel_main(void) {
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

    tree(get_root_inode());

    char buf[10000] = {1};
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
