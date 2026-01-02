#include "exec.h"
#include "interrupt.h"
#include "gdt.h"
#include "io.h"
#include "proc.h"
#include "vga.h"
#include "paging.h"
#include "fs.h"
#include "kprintf.h"
#include "multiboot_defs.h"
#include "pgalloc.h"
#include "util.h"
#include "kstring.h"

void higher_half_entry();

// TODO: add debug print for this
__attribute__ ((section(".boot.data")))
u32 free_pages = 0;

void 
__attribute__ ((section(".boot.text")))
kernel_main(void) {
    struct multiboot_info *multiboot_info;
    __asm__ volatile (
        "mov %%ebx, %0\n"
        : "=r" (multiboot_info)
    );
    if (multiboot_info->flags & (1 << 6)) {
        u32 total_len = 0;
        struct memory_map *entry = (struct memory_map *) multiboot_info->mmap_addr;
        while (total_len < multiboot_info->mmap_length) {
            total_len += entry->size + 4;
            if (entry->type == 1) {
                for (
                    u32 curr_addr = PAGE_ROUND_DOWN(entry->base_addr_low);
                    curr_addr < PAGE_ROUND_DOWN(entry->base_addr_low) + entry->length_low;
                    curr_addr += PGSIZE
                ) {
                    if (curr_addr < 0x400000) continue;
                    u32 entry = (curr_addr / PGSIZE) / 32;
                    u32 offset = (curr_addr / PGSIZE) % 32;
                    page_free_map[entry] |= 1 << offset;
                    free_pages++;
                }
            }
            entry = (struct memory_map *) (((char *) entry) + entry->size + 4);
        }
    } else if (multiboot_info->flags & (1 << 0)) {
        for (
            u32 curr_addr = UPPER_MEM_START;
            curr_addr < UPPER_MEM_START + multiboot_info->mem_upper;
            curr_addr += PGSIZE
        ) {
            u32 entry = (curr_addr / PGSIZE) / 32;
            u32 offset = (curr_addr / PGSIZE) % 32;
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
        "add $0xC0000000, %%esp\n"
        "add $0xC0000000, %%ebp\n"
        : : : "esp", "ebp"
    );

    __asm__ volatile(
        "jmp *%0\n"
        : : "r"((u32) higher_half_entry)
    );

}

char idle_kernel_stack[2 * PGSIZE];
void idle() {
    __asm__ volatile ("int $0x20\n");
    for (;;) {
        __asm__ volatile ("hlt\n");
    }
}

void higher_half_entry() {
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

    // Keyboard setup
    // Disable PS/2 devices
    outb(0x64, 0xAD);
    while (inb(0x64) & 2);
    outb(0x64, 0xA7);
    while (inb(0x64) & 2);
    // Clear keyboard output buffer
    while (inb(0x64) & 1) {
        inb(0x60);
    }
    // Read config byte
    outb(0x64, 0x20);
    while (!(inb(0x64) & 1));
    u8 config = inb(0x60);
    kprintf("Initial PS/2 config byte is %x\n", config);
    config &= 0b10101110;
    // Write config byte
    outb(0x64, 0x60);
    while (inb(0x64) & 2);
    outb(0x60, config);
    while (inb(0x64) & 2);
    // Self-test
    outb(0x64, 0xAA);
    while (!(inb(0x64) & 1));
    u8 self_test_res = inb(0x60);
    if (self_test_res != 0x55) {
        panic("PS/2 controler initialization failed: self-test failed (got 0x%x, expected 0x55)\n", self_test_res);
    }
    // Restore config
    outb(0x64, 0x60);
    while (inb(0x64) & 2);
    outb(0x60, config);
    while (inb(0x64) & 2);
    // Enable devices
    outb(0x64, 0xAE);
    while (inb(0x64) & 2);
    outb(0x64, 0xA8);
    while (inb(0x64) & 2);
    // Enable interrupts
    config |= 0b01000001;
    outb(0x64, 0x60);
    while (inb(0x64) & 2);
    outb(0x60, config);
    while (inb(0x64) & 2);
    kprintf("Modified PS/2 config byte is %x\n", config);

    idt_load();

    kprint(
        "////////                      //               ///   /////         |    \n"
        "////////                     ///                       ///       \\ | *  \n"
        "///       //////  ///  /// ////////  //////  //////    ///      \\ \\|* * \n"
        "//////// ///   //  // ///    ///    //    //   ///     ///       \\ | *  \n"
        "//////// //    //   ////     ///      //////   ///     ///      \\ \\|* * \n"
        "///      //    //   ////     ///    ////  //   ///     ///       \\ | *  \n"
        "///      //   ///  /// //    /// // //   ///   ///     ///        \\|*   \n"
        "///       //////  ///  ///    ////   //////  /////// ///////       |    \n"
        "\n\n"
        "Welcome! Try `ls` to get started.\n"
    );

    struct proc *idle_proc = alloc_proc();
    if (idle_proc == 0) {
        panic("Couldn't allocate idle proc\n");
    }
    strcpy(idle_proc->name, "idle");
    idle_proc->cr3 = (u32) kernel_pgdir;
    idle_proc->status = RUNNABLE;
    idle_proc->kernel_sp = (u32) idle_kernel_stack + 2 * PGSIZE;

    struct inode *sh_inode = get_inode_by_path(get_root_inode(), "sh");
    if (sh_inode == 0) {
        panic("Init program not found");
    }
    struct proc *sh_proc = exec_helper(sh_inode, 0, 0, 0, 0);
    sh_proc_pid = sh_proc->pid;

    idle();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
