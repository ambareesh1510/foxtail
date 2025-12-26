#include "util.h"
#include "kprintf.h"
#include <stdarg.h>

u32 min(u32 a, u32 b) {
    return (a < b) ? a : b;
}

u32 max(u32 a, u32 b) {
    return (a > b) ? a : b;
}

void useless() {
    return;
}

__attribute__ ((noreturn))
void panic(char *msg, ...) {
    va_list args;
    va_start(args, msg);
    kprint("KERNEL PANIC: ");
    vkprintf(msg, args);
    va_end(args);
    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void flush_tlb() {
    __asm__ volatile (
        "mov %%cr3, %%eax\n"
        "mov %%eax, %%cr3\n"
        : : : "%eax"
    );
}
