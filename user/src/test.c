void foo() {
    __asm__ volatile (
        "mov $0xCDCDCDCD, %eax\n"
        "mov $0x85, %ebx\n"
        "mov $0x75, %ecx\n"
        "mov $0x95, %edx\n"
        "int $0x80"
    );
}

void _start() {
    foo();
    while (1) {};
}
