#ifndef STRING_H
#define STRING_H

#include "syscall_defs.h"

unsigned int strlen(const char *str) {
    unsigned int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

unsigned int strcmp(const char *a, const char *b) {
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

int strncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (!a[i] || !b[i]) {
            return a[i] - b[i];
        }
        if (a[i] != b[i]) {
            return a[i] - b[i];
        }
    }
    return 0;
}


void strcpy(char *dst, const char *src) {
    if (dst == 0 || src == 0) {
        return;
    }
    unsigned int i = 0;
    while (1) {
        dst[i] = src[i];
        if (src[i] == '\0') {
            return;
        }
        i++;
    }
}

void memcpy(char *dst, const char *src, unsigned int size) {
    while (size--) {
        *dst++ = *src++;
    }
}

void memset(char *dst, char val, unsigned int size) {
    while (size--) *dst++ = val;
}

int puts(char *str) {
    return write(0, str, strlen(str));
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c)
            return (char *)s;
        s++;
    }

    if (c == '\0')
        return (char *)s;

    return 0;
}

void strcat(char *dst, const char *src) {
    int dst_len = strlen(dst);
    strcpy(dst + dst_len, src);
}

char *trim(char *s) {
    char *start = s;
    char *end;

    // skip leading spaces
    while (*start == ' ')
        start++;

    // string became empty
    if (*start == '\0')
        return start;

    // find end
    end = start;
    while (*end != '\0')
        end++;
    end--;  // now at last char

    // trim trailing spaces
    while (end > start && *end == ' ')
        *end-- = '\0';

    return start;
}

#endif /* STRING_H */
