#include "syscall_defs.h"
void _start() {
    char buf[100];
    int bytes;
    for (;;) {
        write("> ");
        bytes = read(buf, 99);
        buf[bytes] = '\0';
        write(buf);
    }
    exit();


    int pid = getpid();
    if (pid < 4) {
        int other_pid = spawn_proc("test");
        write("start wait!\n");
        wait(other_pid);
        write("done waiting!\n");
    } else {
        write("Hello from test.c!\n");
    }

    write("Next!\n");
    for (int j = 0; j < 10001; j++) { getpid(); }

    exit();
    // while(1);

}
