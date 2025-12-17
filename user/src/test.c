void foo() {
    __asm__ volatile (
        "mov %0, %%eax\n"
        "mov $0x85, %%ebx\n"
        "mov $0x75, %%ecx\n"
        "mov $0x95, %%edx\n"
        "int $0x80"
        : : "r"(100)
    );
}

void _start() {
    foo();
    while (1) {};
}
