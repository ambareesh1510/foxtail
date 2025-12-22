#include "syscall.h"
#include "cleanup.h"
#include "exec.h"
#include "fs.h"
#include "interrupt.h"
#include "kprintf.h"
#include "paging.h"
#include "proc.h"
#include "syscall_defs.h"
#include "vga.h"

// TODO: write a page fault handler that kills the process so that we can't access random memory

bool is_valid_user_addr(uint32_t addr) {
    return addr < HIGHER_HALF_BASE;
}

// Writes the string pointed to by ebx to stdout.
// Returns ebx = 0 on success.
// ebx = -1 if error.
void sys_write(struct syscall_registers *s) {
    if (!is_valid_user_addr(s->ebx)) {
        s->ebx = -1;
        return;
    }
    kprint((char *) s->ebx);
    s->ebx = 0;
    return;
}

// Reads ecx bytes from stdin to the buf at ebx.
// Returns ebx = -1 on error.
// Returns ebx = 0, ecx = # bytes read on success.
void sys_read(struct syscall_registers *s) {
    if (!is_valid_user_addr(s->ebx)) {
        s->ebx = -1;
        return;
    }
    char *buf = (char *) s->ebx;
    uint32_t target = s->ecx;
    uint32_t count = 0;
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
    s->ebx = 0;
    s->ecx = count;
}

// Spawns a new process from the file path specified by ebx.
// Returns ebx = -1 on failure.
// Returns ebx = new pid on success.
void sys_spawn_proc(struct syscall_registers *s) {
    if (!is_valid_user_addr(s->ebx)) {
        s->ebx = -1;
        return;
    }
    // TODO: update base of this to be cwd
    struct inode *prog = get_inode_by_path(get_root_inode(), (char *) s->ebx);
    if (prog == 0) {
        panic("Exec bad inode\n");
    }
    struct proc *new_proc = exec_helper(prog);
    if (new_proc == 0) {
        s->ebx = -1;
    } else {
        new_proc->parent = get_current_proc()->pid;
        s->ebx = new_proc->pid;
    }
}

void sys_getpid(struct syscall_registers *s) {
    s->ebx = get_current_proc()->pid;
}

void sys_exit(struct syscall_registers *s) {
    // TODO: this might not work: sys_exit has a stack frame on the kernel stack, but that kernel stack gets cleaned up in cleanup_proc(). 
    // Instead, we should store kernel stack addr in the proc struct and free it (in scheduler()) once the process is killed.
    cleanup_proc(get_current_proc());
    __asm__ volatile ("int $0x20");
}

// If pid doesn't exist, fail
// If pid isn't child of current process, fail
// Otherwise, set state to waiting, set waiting_proc to pid
void sys_wait(struct syscall_registers *s) {
    struct proc *curr_proc = get_current_proc();
    uint32_t pid = s->ebx;
    bool found = false;
    uint32_t i;
    for (i = 0; i < MAX_PROCS; i++) {
        if (ptable[i].pid == pid) {
            if (ptable[i].parent == get_current_proc()->pid) {
                found = true;
            }
            break;
        }
    }
    if (found) {
        curr_proc->status = WAITING;
        curr_proc->waiting_on = i;
        __asm__ volatile ("int $0x20");
        s->ebx = 0;
    } else {
        s->ebx = -1;
    }
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
        default:
            kprintf("Invalid syscall code: %d\n", s->eax);
            break;
    }
}

