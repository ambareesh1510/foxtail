#include "syscall_defs.h"
#include "printf.h"

void _start(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        int res = harden(argv[i]);
        printf("Hardened\n");
        if (res < 0) {
            printf("Failed to harden path %s\n", argv[i]);
        }
    }
    exit(1);
}
