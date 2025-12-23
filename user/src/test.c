#include "syscall_defs.h"
#include "malloc.h"

struct test {
    int a;
    int b;
    int c;
};

void _start() {
    // struct test *a = (struct test *) sbrk(sizeof(struct test));

    struct test *a = malloc(sizeof(struct test));
    a->a = 1;
    a->b = 2;
    a->c = 99;
    free(a);

    char buf[100];
    int bytes;
    for (;;) {
        write("> ");
        bytes = read(buf, a->c);
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
