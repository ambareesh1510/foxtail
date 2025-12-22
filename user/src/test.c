#include "syscall_defs.h"
void _start() {
    int pid = getpid();
    if (pid < 4) {
        int other_pid = spawn_proc("test");
        write("start wait!\n");
        wait(other_pid);
        write("done waiting!\n");
    } else {
        write("Hello from test.c!\n");
    }

    exit();

    while (1);
}
