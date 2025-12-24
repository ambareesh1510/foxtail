#include "syscall.h"
#include "cleanup.h"
#include "exec.h"
#include "fs.h"
#include "interrupt.h"
#include "kprintf.h"
#include "paging.h"
#include "pgalloc.h"
#include "proc.h"
#include "syscall_defs.h"
#include "util.h"
#include "kstring.h"
#include "vga.h"

// TODO: write a page fault handler that kills the process so that we can't access random memory

bool is_valid_user_addr(u32 addr) {
    return addr < HIGHER_HALF_BASE;
}

// Writes the string pointed to by ebx to stdout.
// Returns eax = 0 on success.
// eax = -1 if error.
void sys_write(struct syscall_registers *s) {
    if (!is_valid_user_addr(s->ebx)) {
        s->eax = -1;
        return;
    }
    kprint((char *) s->ebx);
    s->eax = 0;
    return;
}

// Reads ecx bytes from stdin to the buf at ebx.
// Blocks until at least one byte is available.
// TODO: will need to change blocking behavior when
//   making read() work with files.
// TODO: will need to rethink blocking implementation
//   when adding multiple cores.
// Returns eax = -1 on error.
// Returns eax = # bytes read on success.
void sys_read(struct syscall_registers *s) {
    if (!is_valid_user_addr(s->ebx)) {
        s->eax = -1;
        return;
    }
    if (!input_buffer_nonempty) {
        struct proc *curr_proc = get_current_proc();
        // kprintf("waiting read %d\n", curr_proc->pid);
        curr_proc->status = WAITING_ON_READ;
        __asm__ volatile ("int $0x20");
    }
    char *buf = (char *) s->ebx;
    u32 target = s->ecx;
    u32 count = 0;
    for (; count < target; count++) {
        if (!input_buffer_nonempty) {
            break;
        }
        buf[count] = input_buffer[input_buffer_read_ptr];
        input_buffer_read_ptr = (input_buffer_read_ptr + 1) % INPUT_BUFFER_LEN;
        if (input_buffer_read_ptr == input_buffer_write_ptr) {
            input_buffer_nonempty = false;
        }
    }
    s->eax = count;
}

// Spawns a new process from the file path specified by ebx.
// Returns eax = -1 on failure.
// Returns eax = new pid on success.
void sys_spawn_proc(struct syscall_registers *s) {
    if (!is_valid_user_addr(s->ebx)) {
        s->eax = -1;
        return;
    }
    // TODO: update base of this to be cwd
    struct inode *prog = get_inode_by_path(get_current_proc()->cwd, (char *) s->ebx);
    if (prog == 0) {
        s->eax = -1;
        return;
    }
    struct proc *new_proc = exec_helper(prog);
    if (new_proc == 0) {
        s->eax = -1;
    } else {
        struct proc *curr_proc = get_current_proc();
        new_proc->parent = curr_proc->pid;
        new_proc->cwd = curr_proc->cwd;
        kprintf("Spawn %d\n", new_proc->pid);
        s->eax = new_proc->pid;
    }
}

void sys_getpid(struct syscall_registers *s) {
    s->eax = get_current_proc()->pid;
}

void sys_exit(struct syscall_registers *s) {
    // TODO: this might not work: sys_exit has a stack frame on the kernel stack, but that kernel stack gets cleaned up in cleanup_proc(). 
    // Instead, we should store kernel stack addr in the proc struct and free it (in scheduler()) once the process is killed.
    cleanup_proc(get_current_proc());
    kprintf("Kill %d\n", get_current_proc()->pid);
    __asm__ volatile ("int $0x20");
}

// If pid doesn't exist, fail
// If pid isn't child of current process, fail
// Otherwise, set state to waiting, set waiting_proc to pid
// Return eax = 0 on success, eax = -1 on failure.
void sys_wait(struct syscall_registers *s) {
    struct proc *curr_proc = get_current_proc();
    u32 pid = s->ebx;
    bool found = false;
    u32 i;
    for (i = 0; i < MAX_PROCS; i++) {
        if (ptable[i].pid == pid) {
            if (ptable[i].parent == get_current_proc()->pid) {
                found = true;
            }
            break;
        }
    }
    if (found) {
        curr_proc->status = WAITING_ON_PID;
        curr_proc->waiting_on = i;
        __asm__ volatile ("int $0x20");
        s->eax = 0;
    } else {
        s->eax = -1;
    }
}

u32 sbrk_temp_pgdir[PGDIR_LEN];
void sys_sbrk(struct syscall_registers *s) {
    struct proc *curr_proc = get_current_proc();

    // Round up brk_delta.
    i32 brk_delta = ((s->ebx + PGSIZE - 1) / PGSIZE) * PGSIZE;
    if (brk_delta == 0) {
        return;
    } 

    // Start allocating at 0x80000000.
    kernel_pgtbl[PGDIR_LEN - 1] = curr_proc->cr3 | 0x3;
    flush_tlb();
    memcpy((char *) sbrk_temp_pgdir, (char *) temp_page_ptr, PGSIZE);
    
    if (brk_delta < 0) {
        brk_delta = max(brk_delta, 0x80000000 - curr_proc->brk);
        for (u32 addr = curr_proc->brk - PGSIZE; addr >= curr_proc->brk + brk_delta; addr -= PGSIZE) {
            // Temp-map the pgtbl, dealloc the page
            u32 pgtbl_phys_addr = sbrk_temp_pgdir[(addr >> 22) & 0x03FF];
            kernel_pgtbl[PGDIR_LEN - 1] = pgtbl_phys_addr;
            flush_tlb();
            u32 page_phys_addr = temp_page_ptr[(addr >> 12) & 0x03FF];
            free_page(page_phys_addr / PGSIZE);
            temp_page_ptr[(addr >> 12) & 0x03FF] = 0;
            // If addr is aligned to a pgdir entry boundary, dealloc the pgtbl
            if (addr == ((addr >> 22) << 22)) {
                free_page(pgtbl_phys_addr / PGSIZE);
                sbrk_temp_pgdir[(addr >> 22) & 0x03FF] = 0;
            }
        }
    } else if (brk_delta > 0) {
        for (u32 addr = curr_proc->brk; addr < curr_proc->brk + brk_delta; addr += PGSIZE) {
            u32 pgtbl_phys_addr = sbrk_temp_pgdir[(addr >> 22) & 0x03FF];
            bool new = false;
            // If pgdir doesn't exist, allocate one
            // TODO: you can do this by checking if aligned to a page boundary
            if (!(pgtbl_phys_addr & 0x1)) {
                pgtbl_phys_addr = alloc_page() * PGSIZE;
                sbrk_temp_pgdir[(addr >> 22) & 0x03FF] = pgtbl_phys_addr | 0x7;
                new = true;
            }
            kernel_pgtbl[PGDIR_LEN - 1] = pgtbl_phys_addr | 0x3;
            flush_tlb();
            if (new) {
                memset((char *) temp_page_ptr, 0, PGSIZE);
            }
            temp_page_ptr[(addr >> 12) & 0x03FF] = alloc_page() * PGSIZE | 0x7;
        }
    }

    kernel_pgtbl[PGDIR_LEN - 1] = curr_proc->cr3 | 0x3;
    flush_tlb();
    memcpy((char *) temp_page_ptr, (char *) sbrk_temp_pgdir, PGSIZE);

    // Update brk value in proc struct.
    s->eax = curr_proc->brk;
    curr_proc->brk += brk_delta;
}

// Changes directory to the dir specified in ebx.
// Returns eax = -1 on failure, eax = 0 on success.
void sys_cd(struct syscall_registers *s) {
    if (!is_valid_user_addr(s->ebx)) {
        s->eax = -1;
        return;
    }
    struct proc *curr_proc = get_current_proc();
    struct inode *new_cwd = get_inode_by_path(curr_proc->cwd, (char *) s->ebx);
    if (new_cwd == 0) {
        s->eax = -1;
        return;
    }
    curr_proc->cwd = new_cwd;
    s->eax = 0;
}

// Writes the full path of the pwd into the buf at ebx (whose length is ecx).
// Fails if the buf isn't large enough to accomodate the full path + null terminator.
void sys_pwd(struct syscall_registers *s) {
    // TODO: add "." and ".." entries in directory inode

    // Recurse up the tree and compute the length of the path
    struct proc *curr_proc = get_current_proc();
    u32 len = 0;
    struct inode *curr = curr_proc->cwd;
    while (strcmp(curr->name, "~") != 0) {
        // strlen + path separator ('/')
        len += strlen(curr->name) + 1;
        curr = get_inode_at_idx(curr->parent);
    }
    // strlen + null terminator
    len += strlen(curr->name) + 1;
    if (len > s->ecx) {
        s->eax = -1;
        return;
    }
    // Then copy from the back
    curr = curr_proc->cwd;
    char *buf = (char *) s->ebx;
    len -= 1;
    buf[len] = '\0';
    while (strcmp(curr->name, "~") != 0) {
        u32 name_len = strlen(curr->name);
        len -= name_len;
        memcpy(buf + len, curr->name, name_len);
        len -= 1;
        buf[len] = '/';
        curr = get_inode_at_idx(curr->parent);
    }
    memcpy(buf, curr->name, strlen(curr->name));
    s->eax = 0;
    return;
}

void syscall_interrupt_handler_inner(struct syscall_registers *s) {
    // kprintf("Syscall with eax = %x, ebx = %x, ecx = %x, edx = %x\n", s->eax, s->ebx, s->ecx, s->edx);
    switch (s->eax) {
        case SYS_WRITE:
            sys_write(s);
            break;
        case SYS_READ:
            sys_read(s);
            break;
        case SYS_SPAWN_PROC:
            sys_spawn_proc(s);
            break;
        case SYS_GETPID:
            sys_getpid(s);
            break;
        case SYS_EXIT:
            sys_exit(s);
            break;
        case SYS_WAIT:
            sys_wait(s);
            break;
        case SYS_SBRK:
            sys_sbrk(s);
            break;
        case SYS_CD:
            sys_cd(s);
            break;
        case SYS_PWD:
            sys_pwd(s);
            break;
        default:
            kprintf("Invalid syscall code: %d\n", s->eax);
            break;
    }
}

