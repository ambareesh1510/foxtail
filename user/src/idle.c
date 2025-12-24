#include "syscall_defs.h"

void _start() {
    for (;;) {
        // TODO: this proc should run in kernel space, so it can hlt
        // __asm__ volatile ("hlt");
    }
}
