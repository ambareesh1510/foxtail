#include "string.h"
#include "printf.h"
#include "syscall_defs.h"

void _start() {
    int c_res = create(".", "newfile", SYS_CREATE_FILE);
    if (c_res < 0) {
        puts("failed create file\n");
        exit();
    }
    c_res = create(".", "newdir", SYS_CREATE_DIR);
    if (c_res < 0) {
        puts("failed create dir\n");
        exit();
    }
    int fd = open("newfile", SYS_OPEN_FILE_MODE_READ);
    int d_res = delete("newfile");
    if (d_res < 0) {
        puts("Failed delete newfile\n");
        exit();
    }
    puts("Before close newfile\n");
    close(fd);
    puts("After close newfile\n");
    int res = open("hi/grub.cfg", SYS_OPEN_FILE_MODE_READ | SYS_OPEN_FILE_MODE_WRITE);
    if (res < 0) {
        printf("Failed to open\n");
        exit();
    }
    char buf[1000];
    int read_res_1 = read(res, buf, 999);
    if (read_res_1 < 0) {
        printf("Read 1 failed\n");
        exit();
    }
    buf[read_res_1] = '\0';
    printf("Read 1: %s\n", buf);
    char test[] = "TEST STRING 1234567890";
    res = reopen(0, "hi/grub.cfg", SYS_OPEN_FILE_MODE_WRITE);
    // set_ptr(res, 0);
    int write_res = write(0, test, sizeof(test) - 1);
    if (write_res < 0) {
        printf("Write failed\n");
        exit();
    }

    // printf("wrote %d bytes\n", write_res);
    // set_ptr(res, 0);
    // int read_res_2 = read(res, buf, 999);
    // if (read_res_2 < 0) {
    //     printf("Read 2 failed\n");
    //     exit();
    // }
    // buf[read_res_2] = '\0';
    // printf("Read 2: %s\n", buf);
    exit();
}
