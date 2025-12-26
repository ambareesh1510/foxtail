#include "syscall_defs.h"
#include "string.h"

void _start() {
    char *p = (char *) 0xC0200000;
    puts("How about a magic trick?\n");
    puts(p);
    // *p = 1;
    while (1) {
        puts("HAHAHA!\n");
    }
    exit(0);
}
