#include "sse.h"

void sse_fail() {
    __builtin_trap();
}

// Enable SSE if it exists.
__attribute__((section(".boot.text")))
void enable_sse() {
    // This code is translated directly from
    // https://wiki.osdev.org/SSE#Adding_support
    __asm__ volatile (
        "mov $0x1, %%eax\n"
        "cpuid\n"
        "test %0, %%edx\n"
        "jz .no_sse\n"
        "mov %%cr0, %%eax\n"
        "and $0xFFFB, %%eax\n"
        "or $0x2, %%eax\n"
        "mov %%eax, %%cr0\n"
        "mov %%cr4, %%eax\n"
        "orw %1, %%ax\n"
        "mov %%eax, %%cr4\n"
        "finit\n"
        "fwait\n"
        "jmp .sse_setup_end\n"
        ".no_sse:\n"
        "call sse_fail\n"
        ".sse_setup_end:\n"
        :
        : "i"(1 << 25),
          "i"(3 << 9)
        : "eax", "ebx", "ecx", "edx", "cr0", "cr4", "cc"
    );

    __asm__ volatile (
        "mov $0x1, %%eax\n"
        "cpuid\n"
        "test $(1 << 16), %%edx\n"
        "jz .no_pat\n"
        "mov $0x00070106, %%eax\n"
        "mov $0x00070106, %%edx\n"
        "mov $0x277, %%ecx\n"
        "wrmsr\n"
        "mov %%cr3, %%eax\n"
        "mov %%eax, %%cr3\n"
        ".no_pat:\n"
        :
        :
        : "eax", "ebx", "ecx", "edx", "cc"
    );
}
