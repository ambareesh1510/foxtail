#ifndef STRING_H
#define STRING_H

#include "util.h"

uint32_t strlen(const char *str);
uint32_t strcmp(const char *a, const char *b);
void memcpy(char *dst, const char *src, uint32_t size);
void memset(char *dst, char val, uint32_t size);

#endif /* STRING_H */
