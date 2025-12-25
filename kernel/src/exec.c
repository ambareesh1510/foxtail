#include "exec.h"
#include "elf_defs.h"
#include "fs.h"
#include "fs_defs.h"
#include "gdt.h"
#include "kprintf.h"
#include "paging.h"
#include "pgalloc.h"
#include "kstring.h"
#include "syscall.h"
#include "syscall_defs.h"
#include "proc.h"

u32 new_pgdir[PGDIR_LEN];
u32 old_cr3;
struct elf_header elf_header;
struct inode *prog;
u32 global_argc;
char **global_argv;
char global_arg_buf[PGSIZE];

struct proc *exec_helper(struct inode *prog_ptr, u32 argc, char **argv) {
    prog = prog_ptr;
    global_argc = argc;
    global_argv = argv;
    fs_read_bytes(prog, 0, sizeof(elf_header), (char *) (&elf_header));

    if (elf_header.magic != ELF_MAGIC) {
        kprintf("Exec %s: bad magic\n", prog->name);
        return 0;
    }
    struct proc *new_proc = alloc_proc();
    memcpy(new_proc->name, prog->name, FILENAME_MAX_LEN);

    // Use the last entry of kernel_pgtbl as a temporary buffer.
    // TODO: maybe move these to global scope, since they're also used in cleanup?
    
    // uint32_t new_pgdir[PGDIR_LEN] = {0};
    memset((char *) new_pgdir, 0, PGSIZE);

    for (u32 i = 0; i < elf_header.phnum; i++) {
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
            u32 seg_addr = PAGE_ROUND_DOWN(program_header.vaddr);
            seg_addr < PAGE_ROUND_DOWN(program_header.vaddr + program_header.memsz) + PGSIZE;
            seg_addr += PGSIZE
        ) {
            u32 pgtbl_addr;
            bool new = false;
            // If there's no page table, allocate one
            if (new_pgdir[seg_addr >> 22] == 0) {
                u32 new_page_table_addr = alloc_page() * PGSIZE;
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
                u32 new_page_addr = alloc_page() * PGSIZE;

                temp_page_ptr[(seg_addr >> 12) & 0x000003FF] = new_page_addr | 0x7;
            }
        }

    }

    // Allocate a stack and set up stack pointer
    u32 stack_addr = alloc_page() * PGSIZE;
    u32 kernel_stack_top_addr = alloc_page() * PGSIZE;
    u32 kernel_stack_bottom_addr = alloc_page() * PGSIZE;
    u32 stack_pgtbl_addr;
    u32 stack_vaddr_low = HIGHER_HALF_BASE - PGSIZE;
    // If there's no page table, allocate one
    bool new = false;
    if (new_pgdir[stack_vaddr_low >> 22] == 0) {
        u32 new_stack_page_table_addr = alloc_page() * PGSIZE;
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

    // Allocate space for argc, argv
    u32 total_argv_len = 0;
    for (u32 i = 0; i < argc; i++) {
        char *arg = argv[i];
        // One extra byte for null terminator
        // Add sizeof(char *) to account for pointer in array
        total_argv_len += strlen(arg) + 1 + sizeof(char *);
    }

    // TODO: this limits the size of argvs to 1 page (because I'm lazy).
    // Implement this properly at some point.
    char **argv_arr = (char **) global_arg_buf;
    char *argv_buf = (char *) global_arg_buf + global_argc * sizeof(char *);
    for (u32 i = 0; i < global_argc; i++) {
        u32 next_len = strlen(global_argv[i]) + 1;
        if ((u32) argv_buf + next_len > (u32) global_arg_buf + PGSIZE) {
            global_argc = i;
            break;
        }
        argv_arr[i] = (char *) (0x80000000 + (u32) argv_buf - (u32) global_arg_buf);
        strcpy(
            argv_buf,
            global_argv[i]
        );
        argv_buf += next_len;
    }
    
    new_pgdir[HIGHER_HALF_BASE >> 22] = ((u32) ((char *) kernel_pgtbl - HIGHER_HALF_BASE) & 0xfffff000) | 0x3;
    
    // Copy the new pgdir into newly allocated page
    u32 new_pgdir_addr = alloc_page() * PGSIZE;
    kernel_pgtbl[PGTBL_LEN - 1] = new_pgdir_addr | 0x3;
    flush_tlb();
    memcpy((char *) temp_page_ptr, (char *) new_pgdir, PGSIZE);
    kernel_pgtbl[PGTBL_LEN - 1] = (u32) ((char *) kernel_pgtbl - HIGHER_HALF_BASE) | 0x3;

    new_proc->cr3 = new_pgdir_addr;
    // TODO: remove magic number
    new_proc->brk = 0x80000000;

    // TODO: using sbrk_helper is not efficient (should do it before the cr3 is loaded into new_proc)
    sbrk_helper(new_proc, total_argv_len);
    
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
    for (u32 i = 0; i < elf_header.phnum; i++) {
        struct program_header program_header;
        u32 ph_bytes = fs_read_bytes(
            prog,
            elf_header.phoff + i * sizeof(struct program_header),
            sizeof(program_header),
            (char *) (&program_header)
        );
        fs_read_bytes(prog, program_header.offset, program_header.filesz, (char *) program_header.vaddr);
        memset((char *) (program_header.vaddr + program_header.filesz), 0, program_header.memsz - program_header.filesz);
    }

    // TODO: see previous comment about implementing argc/argv properly
    if (global_argc > 0) {
        memcpy(
            (char *) 0x80000000,
            global_arg_buf,
            PGSIZE
        );
    }

    // Push argc, argv onto the stack
    u32 *stack = (u32 *) HIGHER_HALF_BASE;
    stack--;
    *stack = (u32) (0x80000000);
    stack--;
    *stack = global_argc;
    stack--;
    *stack = 0;
    stack--;
    *stack = 0;

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
    new_proc->registers.cs = 0x1B;
    new_proc->registers.ss = 0x23;
    new_proc->registers.esp = HIGHER_HALF_BASE - 12;
    new_proc->registers.ebp = HIGHER_HALF_BASE;
    new_proc->registers.eip = elf_header.entry;

    new_proc->status = EMBRYO;
    new_proc->cwd = get_root_inode();
    // Fill fds
    new_proc->fds[0] = (struct fd) {
        .status = FD_STDOUT,
        .mode = SYS_OPEN_FILE_MODE_WRITE,
        .file = 0,
        .ptr = 0,
    };
    new_proc->fds[1] = (struct fd) {
        .status = FD_STDIN,
        .mode = SYS_OPEN_FILE_MODE_READ,
        .file = 0,
        .ptr = 0,
    };
    new_proc->fds[2] = (struct fd) {
        .status = FD_STDERR,
        .mode = SYS_OPEN_FILE_MODE_WRITE,
        .file = 0,
        .ptr = 0,
    };
    for (u32 i = 3; i < MAX_FDS; i++) {
        new_proc->fds[i].status = FD_UNMAPPED;
    }
    new_proc->present = true;

    // kprintf("new proc name=%s pid=%d: eip=%x cs=%x eflags=%x esp=%x ss=%x\n",
    //         new_proc->name,
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

    struct proc *new_proc = exec_helper(prog, 0, 0);
    if (new_proc == 0) {
        panic("Exec helper failed\n");
    }
    new_proc->status = RUNNABLE;
    new_proc->parent = 0xFFFFFFFF;
    scheduler_proc_index = ((u32) new_proc - (u32) ptable) / sizeof(struct proc);

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
