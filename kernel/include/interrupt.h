#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "util.h"

extern char kb_char;
extern volatile uint32_t ticks;

#define INPUT_BUFFER_LEN 1024
extern char input_buffer[INPUT_BUFFER_LEN];
extern uint32_t input_buffer_write_ptr;
extern uint32_t input_buffer_read_ptr;
extern bool input_buffer_nonempty;

void idt_load();

#endif /* INTERRUPT_H */
