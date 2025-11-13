#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "util.h"

extern char kb_char;
extern volatile uint32_t ticks;
void idt_load();

#endif /* INTERRUPT_H */
