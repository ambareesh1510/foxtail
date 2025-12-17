#include "exec.h"
#include "elf_defs.h"
#include "fs.h"
#include "kprintf.h"
#include "paging.h"
#include "pgalloc.h"
#include "kstring.h"

enum exec_status exec(struct inode *prog, uint32_t *kernel_pgtbl, uint32_t *kernel_pgdir) {
    if (prog->type == FT_DIRECTORY) {
        return EXEC_ERROR_FT_DIRECTORY;
    }

    struct elf_header elf_header;
    fs_read_bytes(prog, 0, sizeof(elf_header), (char *) (&elf_header));
    
    if (elf_header.magic != ELF_MAGIC) {
        return EXEC_ERROR_INVALID_MAGIC;
    }

    // Use the last entry of kernel_pgtbl as a temporary buffer.
    uint32_t *temp_page_ptr = (uint32_t *) (HIGHER_HALF_BASE + PGSIZE * (PGDIR_LEN - 1));
    
    uint32_t new_pgdir[PGDIR_LEN] = {0};

    for (uint32_t i = 0; i < elf_header.phnum; i++) {
        struct program_header program_header;
        fs_read_bytes(
            prog,
            elf_header.phoff + i * sizeof(struct program_header),
            sizeof(program_header),
            (char *) (&program_header)
        );

        // We only allocate pages here; the copy happens after the pgtbl is updated.

        // 1. Allocate page
        // 2. add new page to page table using temp mapping
        // 3. if necessary, add page table to pgdir
        for (
            uint32_t seg_addr = PAGE_ROUND_DOWN(program_header.vaddr);
            seg_addr < PAGE_ROUND_DOWN(program_header.vaddr + program_header.memsz) + PGSIZE;
            seg_addr += PGSIZE
        ) {
            // TODO: shouldn't alloc a new page if there already exists one (e.g. from a previous segment)
            // uint32_t _ = alloc_page();
            uint32_t new_page_addr = alloc_page() * PGSIZE;

            uint32_t pgtbl_addr;
            // If there's no page table, allocate one
            if (new_pgdir[seg_addr >> 22] == 0) {
                uint32_t new_page_table_addr = alloc_page() * PGSIZE;
                new_pgdir[seg_addr >> 22] = (new_page_table_addr & 0xfffff000) | 0x3;
                pgtbl_addr = new_page_table_addr;
            } else {
                pgtbl_addr = new_pgdir[seg_addr >> 22] & 0xfffff000;
            }
            kprintf("Kernel pgtbl addr = %x\n", kernel_pgtbl);
            kernel_pgtbl[PGTBL_LEN - 1] = pgtbl_addr | 0x3;
            // __asm__ volatile ("invlpg (%0)" : : "r" (temp_page_ptr) : "memory");
            __asm__ volatile (
                "mov %cr3, %eax\n"
                "mov %eax, %cr3\n"
            );
            temp_page_ptr[(seg_addr >> 12) & 0x000003FF] = new_page_addr | 0x3;
            kprintf("accessing %x\n", &temp_page_ptr[(seg_addr >> 12) & 0x000003FF]);
        }

    }

    // Allocate a stack and set up stack pointer
    uint32_t stack_addr = alloc_page() * PGSIZE;
    uint32_t stack_pgtbl_addr;
    uint32_t stack_vaddr_low = HIGHER_HALF_BASE - PGSIZE;
    // If there's no page table, allocate one
    if (new_pgdir[stack_vaddr_low >> 22] == 0) {
        uint32_t new_stack_page_table_addr = alloc_page() * PGSIZE;
        new_pgdir[stack_vaddr_low >> 22] = (new_stack_page_table_addr & 0xfffff000) | 0x3;
        stack_pgtbl_addr = new_stack_page_table_addr;
    } else {
        stack_pgtbl_addr = new_pgdir[stack_vaddr_low >> 22] & 0xfffff000;
    }
    kernel_pgtbl[PGTBL_LEN - 1] = stack_pgtbl_addr | 0x3;
    __asm__ volatile (
        "mov %cr3, %eax\n"
        "mov %eax, %cr3\n"
    );
    temp_page_ptr[PGTBL_LEN - 1] = stack_addr | 0x3;
    
    new_pgdir[HIGHER_HALF_BASE >> 22] = ((uint32_t) ((char *) kernel_pgtbl - HIGHER_HALF_BASE) & 0xfffff000) | 0x3;
    
    // Copy the new pgdir into newly allocated page
    uint32_t new_pgdir_addr = alloc_page() * PGSIZE;
    kernel_pgtbl[PGTBL_LEN - 1] = new_pgdir_addr | 0x3;
    __asm__ volatile (
        "mov %cr3, %eax\n"
        "mov %eax, %cr3\n"
    );
    kprintf("memcpy to %x from %x\n", temp_page_ptr, new_pgdir);
    memcpy((char *) temp_page_ptr, (char *) new_pgdir, PGSIZE);
    kernel_pgtbl[PGTBL_LEN - 1] = (uint32_t) ((char *) kernel_pgtbl - HIGHER_HALF_BASE) | 0x3;
    
    __asm__ volatile (
        "mov %0, %%eax\n"
        "mov %%eax, %%cr3\n"

        "mov %%cr0, %%eax\n"
        "orl $0x80000001, %%eax\n"
        "mov %%eax, %%cr0\n"
        :
        : "r"(new_pgdir_addr)
        : "%eax"
    );

    // Copy each segment into memory
    for (uint32_t i = 0; i < elf_header.phnum; i++) {
        struct program_header program_header;
        fs_read_bytes(
            prog,
            elf_header.phoff + i * sizeof(struct program_header),
            sizeof(program_header),
            (char *) (&program_header)
        );
        fs_read_bytes(prog, program_header.offset, program_header.memsz, (char *) program_header.vaddr);
    }

    // Update stack pointer to process stack
    __asm__ volatile (
        "mov %0, %%esp\n"
        : : "r"(HIGHER_HALF_BASE - 1)
    );

    // Jump to process entry point
    __asm__ volatile(
        "jmp *%0\n"
        : : "r"(elf_header.entry)
    );

    return EXEC_SUCCESS;
}
