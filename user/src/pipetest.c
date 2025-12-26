#include "syscall_defs.h"
#include "string.h"
#include "printf.h"

void _start() {
    struct pipe my_pipe;
    int pipe_res = pipe(&my_pipe);
    if (pipe_res < 0) {
        puts("failed to create pipe\n");
    }
    printf("got fds read=%d, write=%d\n", my_pipe.read_fd, my_pipe.write_fd);

    char str[] = "Hello\nThis is a thing\na";
    int write_res = write(my_pipe.write_fd, str, sizeof(str) - 1);
    if (write_res < 0) {
        puts("failed to write to pipe\n");
    }
    char buf[100];
    int read_res = read(my_pipe.read_fd, buf, 99);
    if (read_res < 0) {
        puts("failed to read from pipe\n");
    }
    printf("read %d bytes from pipe\n", read_res);
    buf[read_res] = 0;
    puts(buf);

    exit(0);
}
