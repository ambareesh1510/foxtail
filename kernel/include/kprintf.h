#ifndef KPRINTF_H
#define KPRINTF_H

#include <stdarg.h>

void kprint_char(char c);
void kprint(const char *str);
void kprintf(const char *fmt, ...);
void vkprintf(const char *fmt, va_list args);

#endif /* KPRINTF_H */
