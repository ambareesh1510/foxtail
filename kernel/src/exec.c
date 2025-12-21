#include "exec.h"
#include "elf_defs.h"
#include "fs.h"
#include "fs_defs.h"
#include "gdt.h"
#include "kprintf.h"
#include "paging.h"
#include "pgalloc.h"
#include "kstring.h"
#include "proc.h"

uint32_t new_pgdir[PGDIR_LEN];
uint32_t old_cr3;
struct elf_header elf_header;
struct inode *prog;

struct proc *exec_helper(struct inode *prog_ptr) {
    prog = prog_ptr;
    fs_read_bytes(prog, 0, sizeof(elf_header), (char *) (&elf_header));

    if (elf_header.magic != ELF_MAGIC) {
        kprintf("Bad magic\n");
        return 0;
    }
    struct proc *new_proc = alloc_proc();
    memcpy(new_proc->name, prog->name, FILENAME_MAX_LEN);

    // TODO: allocate argc, argv

    // Use the last entry of kernel_pgtbl as a temporary buffer.
    // TODO: maybe move these to global scope, since they're also used in cleanup?
    uint32_t *kernel_pgtbl = (uint32_t *) ((char *) kernel_id_pgtbl + HIGHER_HALF_BASE);
    uint32_t *temp_page_ptr = (uint32_t *) (HIGHER_HALF_BASE + PGSIZE * (PGDIR_LEN - 1));
    
    // uint32_t new_pgdir[PGDIR_LEN] = {0};
    memset((char *) new_pgdir, 0, PGSIZE);

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
            uint32_t pgtbl_addr;
            bool new = false;
            // If there's no page table, allocate one
            if (new_pgdir[seg_addr >> 22] == 0) {
                uint32_t new_page_table_addr = alloc_page() * PGSIZE;
                new_pgdir[seg_addr >> 22] = (new_page_table_addr & 0xfffff000) | 0x7;
                pgtbl_addr = new_page_table_addr;
                new = true;
            } else {
                pgtbl_addr = new_pgdir[seg_addr >> 22] & 0xfffff000;
            }
            kernel_pgtbl[PGTBL_LEN - 1] = pgtbl_addr | 0x3;
            flush_tlb();
            // If we just allocated a new page table, zero it out.
            if (new) {
                memset((char *) temp_page_ptr, 0, PGSIZE);
            }

            if (temp_page_ptr[(seg_addr >> 12) & 0x000003FF] == 0) {
                uint32_t new_page_addr = alloc_page() * PGSIZE;

                temp_page_ptr[(seg_addr >> 12) & 0x000003FF] = new_page_addr | 0x7;
            }
        }

    }

    // Allocate a stack and set up stack pointer
    uint32_t stack_addr = alloc_page() * PGSIZE;
    uint32_t kernel_stack_top_addr = alloc_page() * PGSIZE;
    uint32_t kernel_stack_bottom_addr = alloc_page() * PGSIZE;
    uint32_t stack_pgtbl_addr;
    uint32_t stack_vaddr_low = HIGHER_HALF_BASE - PGSIZE;
    // If there's no page table, allocate one
    bool new = false;
    if (new_pgdir[stack_vaddr_low >> 22] == 0) {
        uint32_t new_stack_page_table_addr = alloc_page() * PGSIZE;
        new_pgdir[stack_vaddr_low >> 22] = (new_stack_page_table_addr & 0xfffff000) | 0x7;
        stack_pgtbl_addr = new_stack_page_table_addr;
        new = true;
    } else {
        stack_pgtbl_addr = new_pgdir[stack_vaddr_low >> 22] & 0xfffff000;
    }
    kernel_pgtbl[PGTBL_LEN - 1] = stack_pgtbl_addr | 0x3;
    flush_tlb();

    // If we just allocated a new page table, zero it out.
    if (new) {
        memset((char *) temp_page_ptr, 0, PGSIZE);
    }
    temp_page_ptr[PGTBL_LEN - 1] = stack_addr | 0x7;
    temp_page_ptr[PGTBL_LEN - 2] = kernel_stack_top_addr | 0x3;
    temp_page_ptr[PGTBL_LEN - 3] = kernel_stack_bottom_addr | 0x3;
    new_proc->kernel_stack = kernel_stack_top_addr;
    
    new_pgdir[HIGHER_HALF_BASE >> 22] = ((uint32_t) ((char *) kernel_pgtbl - HIGHER_HALF_BASE) & 0xfffff000) | 0x3;
    
    // Copy the new pgdir into newly allocated page
    uint32_t new_pgdir_addr = alloc_page() * PGSIZE;
    kernel_pgtbl[PGTBL_LEN - 1] = new_pgdir_addr | 0x3;
    flush_tlb();
    memcpy((char *) temp_page_ptr, (char *) new_pgdir, PGSIZE);
    kernel_pgtbl[PGTBL_LEN - 1] = (uint32_t) ((char *) kernel_pgtbl - HIGHER_HALF_BASE) | 0x3;
    
    __asm__ volatile (
        "mov %%cr3, %%eax\n"
        "mov %%eax, %0\n"
        : "=m"(old_cr3)
        : : "%eax"
    );

    __asm__ volatile (
        "mov %0, %%eax\n"
        "mov %%eax, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "orl $0x80000001, %%eax\n"
        "mov %%eax, %%cr0\n"
        :
        : "r"(new_pgdir_addr)
        : "eax", "cr3"
    );


    // Copy each segment into memory
    for (uint32_t i = 0; i < elf_header.phnum; i++) {
        struct program_header program_header;
        uint32_t ph_bytes = fs_read_bytes(
            prog,
            elf_header.phoff + i * sizeof(struct program_header),
            sizeof(program_header),
            (char *) (&program_header)
        );
        fs_read_bytes(prog, program_header.offset, program_header.memsz, (char *) program_header.vaddr);
    }

    // Restore the old cr3
    __asm__ volatile (
        "mov %0, %%eax\n"
        "mov %%eax, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "orl $0x80000001, %%eax\n"
        "mov %%eax, %%cr0\n"
        :
        : "m"(old_cr3)
        : "%eax", "cr3"
    );

    __asm__ volatile (
        "pushf\n"                    // eflags
        "popl %%eax\n"
        "orl $0x200, %%eax\n"        // Enable interrupts by setting appropriate flag
        "mov %%eax, %0\n"
        : "=m"(new_proc->registers.eflags)
        : : "eax"
    );
    

    // Copy new process's details into the proc struct
    new_proc->cr3 = new_pgdir_addr;
    new_proc->registers.cs = 0x1B;
    new_proc->registers.ss = 0x23;
    new_proc->registers.esp = HIGHER_HALF_BASE;
    new_proc->registers.ebp = HIGHER_HALF_BASE;
    new_proc->registers.eip = elf_header.entry;

    new_proc->status = EMBRYO;
    new_proc->present = true;

    // kprintf("new proc pid=%d: eip=%x cs=%x eflags=%x esp=%x ss=%x\n",
    //         new_proc->pid,
    //         new_proc->registers.eip, new_proc->registers.cs, new_proc->registers.eflags, new_proc->registers.esp, new_proc->registers.ss);
    return new_proc;
}

enum exec_status exec(struct inode *prog) {
    if (prog->type == FT_DIRECTORY) {
        return EXEC_ERROR_FT_DIRECTORY;
    }

    struct elf_header elf_header;
    fs_read_bytes(prog, 0, sizeof(elf_header), (char *) (&elf_header));
    
    if (elf_header.magic != ELF_MAGIC) {
        return EXEC_ERROR_INVALID_MAGIC;
    }

    struct proc *new_proc = exec_helper(prog);
    if (new_proc == 0) {
        panic("Exec helper failed\n");
    }
    new_proc->status = RUNNABLE;

    // Ring 3 transition
    tss.esp0 = HIGHER_HALF_BASE - PGSIZE;
    __asm__ volatile (
        "mov %0, %%eax\n"
        "mov %%eax, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "orl $0x80000001, %%eax\n"
        "mov %%eax, %%cr0\n"
        :
        : "r"(new_proc->cr3)
        : "eax"
    );
    __asm__ volatile(
        "cli\n"
        
        // User data segment is 0x20; OR with Requested Privilege Level (RPL = 0x3)
        "mov $0x23, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        
        "pushl $0x23\n"              // User data segment
        "pushl %0\n"                 // eip (user stack)
        "pushf\n"                    // eflags
        "popl %%eax\n"
        "orl $0x200, %%eax\n"        // Enable interrupts by setting appropriate flag
        "pushl %1\n"              // Push modified eflags
        "pushl $0x1B\n"              // User code segment
        "pushl %2\n"                 // esp
        "sti\n"
        
        "iret\n"                     // iret to ring 3
        : : "r"(new_proc->registers.esp), "r"(new_proc->registers.eflags), "r"(new_proc->registers.eip) : "eax"
    );

    return EXEC_SUCCESS;
}
