#ifndef STRING_H
#define STRING_H

#include "util.h"

u32 strlen(const char *str);
u32 strcmp(const char *a, const char *b);
u32 strcpy(char *dst, char *src);
void memcpy(char *dst, const char *src, u32 size);
void memset(char *dst, char val, u32 size);

#endif /* STRING_H */
