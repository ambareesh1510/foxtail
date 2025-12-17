#ifndef SYSCALL_H
#define SYSCALL_H

#include "util.h"

struct syscall_registers {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
};

void syscall_interrupt_handler_inner(struct syscall_registers *s);

#endif /* SYSCALL_H */
