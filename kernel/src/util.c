#include "util.h"
#include "kprintf.h"

uint32_t min(uint32_t a, uint32_t b) {
    return (a < b) ? a : b;
}

uint32_t max(uint32_t a, uint32_t b) {
    return (a > b) ? a : b;
}

void panic(char *msg) {
    kprintf("KERNEL PANIC: %s\n", msg);
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
