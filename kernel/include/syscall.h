#ifndef SYSCALL_H
#define SYSCALL_H

#include "util.h"
#include "proc.h"

#define PIPE_BUF_SIZE (4096 * 4)
#define NUM_PIPE_BUFS 16

extern char pipe_buffers[NUM_PIPE_BUFS][PIPE_BUF_SIZE];

struct pipe_data {
    u32 num_refs;
    u32 read_ptr;
    u32 write_ptr;
    u32 internal_write_ptr;
};
extern struct pipe_data pipe_data[NUM_PIPE_BUFS];

struct syscall_registers {
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;
};

void syscall_interrupt_handler_inner(struct syscall_registers *s);
void sbrk_helper(struct proc *curr_proc, i32 brk_delta);

#endif /* SYSCALL_H */
