#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "util.h"
#include "syscall_defs.h"

extern volatile u32 ticks;

#define INPUT_BUFFER_LEN 1024
struct tty_data {
    enum tty_mode mode;
    char input_buffer[INPUT_BUFFER_LEN];
    u32 read_ptr;
    u32 write_ptr;
    u32 internal_write_ptr;
    bool left_shift;
    bool right_shift;
};
extern struct tty_data tty_data;

void idt_load();

#endif /* INTERRUPT_H */
