#ifndef IO_H
#define IO_H

#include "util.h"

u8 inb(u16 port);

void outb(u16 port, u8 val);

#endif /* IO_H */
