#include "kstring.h"
#include "kprintf.h"

u32 strlen(const char *str) {
    u32 len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

u32 strcmp(const char *a, const char *b) {
    int i = 0;
    for (;;) {
        if (a[i] == '\0' || b[i] == '\0') {
            if (a[i] == '\0' && b[i] == '\0') {
                return 0;
            }
            return 1;
        }
        if (a[i] != b[i]) {
            return 1;
        }
        i++;
    }
}

u32 strcpy(char *dst, char *src) {
    if (dst == 0 || src == 0) {
        return 0;
    }
    u32 i = 0;
    while (1) {
        dst[i] = src[i];
        if (src[i] == '\0') {
            return i;
        }
        i++;
    }
}

void memcpy(char *dst, const char *src, u32 size) {
    while (size--) {
        *dst++ = *src++;
    }
}

void memset(char *dst, char val, u32 size) {
    while (size--) *dst++ = val;
}

void memmove(void *dest, const void *src, u32 n) {
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;

    if (d == s || n == 0) {
        return;
    }

    if (d < s) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = n; i != 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
}

