#include "util.h"
#include "kprintf.h"

uint32_t min(uint32_t a, uint32_t b) {
    return (a < b) ? a : b;
}

uint32_t max(uint32_t a, uint32_t b) {
    return (a > b) ? a : b;
}

void useless() {
    return;
}

__attribute__ ((noreturn))
void panic(char *msg) {
    kprintf("KERNEL PANIC: %s\n", msg);
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
