#include "syscall.h"
#include "interrupt.h"
#include "kprintf.h"
#include "paging.h"
#include "syscall_defs.h"

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
    // if (count != 0) 
    //     kprintf("read %d\n", count);
    s->ecx = count;
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
        default:
            kprintf("Invalid syscall code: %d\n", s->eax);
            break;
    }
}

