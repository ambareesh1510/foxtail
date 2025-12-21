#include "syscall_defs.h"
void _start() {
    char *a = "Hello from test.c!\n";
    __asm__ volatile (
        "mov $0x0, %%eax\n"
        "mov %0, %%ebx\n"
        "int $0x80"
        : : "r"(a) : "eax", "ebx"
    );

    __asm__ volatile (
        "int $0x80"
        : :
        "a"(SYS_GETPID)
    );
    int pid;
    __asm__ volatile (
        "mov %%ebx, %0"
        : "=m"(pid)
    );
    if (pid < 1) {
        char *name = "test";
        __asm__ volatile (
            "mov $0x02, %%eax\n"
            "mov %0, %%ebx\n"
            "int $0x80\n"
            : :
            "m"(name)
        );
        __asm__ volatile (
            "mov $0x02, %%eax\n"
            "mov %0, %%ebx\n"
            "int $0x80\n"
            : :
            "m"(name)
        );
        __asm__ volatile (
            "mov $0x02, %%eax\n"
            "mov %0, %%ebx\n"
            "int $0x80\n"
            : :
            "m"(name)
        );
        __asm__ volatile (
            "mov $0x02, %%eax\n"
            "mov %0, %%ebx\n"
            "int $0x80\n"
            : :
            "m"(name)
        );
    }

    volatile int i = 0;
    while (1) {
        i++;
        // char buf[10] = {0};
        // __asm__ volatile (
        //     "mov $0x1, %%eax\n"
        //     "mov %0, %%ebx\n"
        //     "mov %1, %%ecx\n"
        //     "int $0x80"
        //     : :
        //     "r"(buf), "r"(9)
        //     : "eax", "ebx", "ecx"
        // );
        // // __asm__ volatile (
        //     // "mov $0x0, %%eax\n"
        //     // "mov %0, %%ebx\n"
        //     "int $0x80"
        //     : :
        //     "a"(0),
        //     "b"(buf)
        //     // : "eax", "ebx"
        // );
    };
}
