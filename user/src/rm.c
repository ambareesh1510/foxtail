#include "syscall_defs.h"
#include "printf.h"

void _start(int argc, char **argv) {
    if (argc < 2) {
        puts("Expected at least one argument\n");
        exit(1);
    }
    for (int i = 1; i < argc; i++) {
        int res = delete(argv[i]);
        if (res < 0) {
            printf("Couldn't delete %s\n", argv[i]);
        }
    }
    exit(0);
}
