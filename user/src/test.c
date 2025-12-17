void _start() {
    __asm__ volatile (
        "mov $0xABABCDCD, %eax\n"
        "mov $0x85, %ebx\n"
        "mov $0x75, %ecx\n"
        "mov $0x95, %edx\n"
        "int $0x80"
    );
    while (1) {};
}
