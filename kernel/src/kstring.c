#include "kstring.h"

uint32_t strlen(const char *str) {
    uint32_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

uint32_t strcmp(const char *a, const char *b) {
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

void memcpy(char *dst, const char *src, uint32_t size) {
    while (size--) *dst++ = *src++;
}

void memset(char *dst, char val, uint32_t size) {
    while (size--) *dst++ = val;
}
