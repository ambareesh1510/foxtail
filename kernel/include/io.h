#ifndef IO_H
#define IO_H

#include "util.h"

__attribute__ ((no_caller_saved_registers))
u8 inb(u16 port);

__attribute__ ((no_caller_saved_registers))
void outb(u16 port, u8 val);

#endif /* IO_H */
