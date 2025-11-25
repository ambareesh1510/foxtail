#ifndef IO_H
#define IO_H

#include "util.h"

__attribute__ ((no_caller_saved_registers))
uint8_t inb(uint16_t port);

__attribute__ ((no_caller_saved_registers))
void outb(uint16_t port, uint8_t val);

#endif /* IO_H */
