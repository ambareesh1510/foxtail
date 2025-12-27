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
    u32 mem_lower;
    u32 mem_upper;
    u32 mmap_len;
    u32 mmap_addr;
};

void higher_half_entry();

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
        "add $0xC0000000, %esp\n"
        "add $0xC0000000, %ebp\n"
    );


    // __asm__ volatile (
    //     "calll %0\n"
    //     : : "r"(higher_half_entry)
    // );
    __asm__ volatile(
        "jmp *%0\n"
        : : "r"((u32) higher_half_entry)
    );
    // higher_half_entry();
//     void (*hh_entry)(void) =
//     (void (*)(void))((uint32_t)higher_half_entry + HIGHER_HALF_BASE);
//
// hh_entry();

}

void idle() {
    __asm__ volatile ("int $0x20\n");
    for (;;) {
        __asm__ volatile ("hlt\n");
    }
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

    // kprintf("Total free pages: %d\n", *(u32 *) ((char *) &free_pages + HIGHER_HALF_BASE));

    kprint(
        "////////                      //               ///   /////         |    \n"
        "////////                     ///                       ///       \\ | /  \n"
        "///       //////  ///  /// ////////  //////  //////    ///      \\ \\|/ / \n"
        "//////// ///   //  // ///    ///    //    //   ///     ///       \\ | /  \n"
        "//////// //    //   ////     ///      //////   ///     ///      \\ \\|/ / \n"
        "///      //    //   ////     ///    ////  //   ///     ///       \\ | /  \n"
        "///      //   ///  /// //    /// // //   ///   ///     ///        \\|/   \n"
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
    idle_proc->kernel_sp = (u32) a_kernel_stack + 2 * PGSIZE;

    struct inode *sh_inode = get_inode_by_path(get_root_inode(), "sh");
    if (sh_inode == 0) {
        panic("Init program not found");
    }
    exec_helper(sh_inode, 0, 0, 0, 0);

    idle();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
