#ifndef SYSCALL_H
#define SYSCALL_H

#include "util.h"
#include "proc.h"

struct syscall_registers {
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;
};

void syscall_interrupt_handler_inner(struct syscall_registers *s);
void sbrk_helper(struct proc *curr_proc, i32 brk_delta);

#endif /* SYSCALL_H */
