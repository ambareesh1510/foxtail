void _start() {
    char *a = "Hello from test.c!\n";
    __asm__ volatile (
        "mov $0x0, %%eax\n"
        "mov %0, %%ebx\n"
        "int $0x80"
        : : "r"(a) : "eax", "ebx"
    );

    while (1) {
        char buf[10] = {0};
        __asm__ volatile (
            "mov $0x1, %%eax\n"
            "mov %0, %%ebx\n"
            "mov %1, %%ecx\n"
            "int $0x80"
            : :
            "r"(buf), "r"(9)
            : "eax", "ebx", "ecx"
        );
        // __asm__ volatile (
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
