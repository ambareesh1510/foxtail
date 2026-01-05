#ifndef KPRINTF_H
#define KPRINTF_H

#include <stdarg.h>
#include "util.h"

void kprint_char(char c);
void kprint(const char *str);
void kprintf(const char *fmt, ...);
void vkprintf(const char *fmt, va_list args);
void move_cursor(u32 row, u32 col);

#endif /* KPRINTF_H */
