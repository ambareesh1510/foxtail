#include "syscall_defs.h"
#include "printf.h"

#define BUF_LEN 1000
void cat(char *path) {
    int fd = open(path, SYS_OPEN_FILE_MODE_READ);
    if (fd < 0) {
        printf("Failed to open path %s\n", path);
        return;
    }
    int ft = ftype(fd);
    if (ft != SYS_FTYPE_FILE) {
        printf("Path %s is not a file\n", path);
        return;
    }
    struct file_info info;
    file_info(fd, &info);
    char buf[BUF_LEN + 1];
    for (unsigned int i = 0; i < (info.size + BUF_LEN - 1) / BUF_LEN; i++) {
        int bytes = read(fd, buf, BUF_LEN);
        buf[bytes] = '\0';
        puts(buf);
    }
    puts("\n");
}

void _start(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        cat(argv[i]);
    }
    exit();
}
