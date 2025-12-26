#include "syscall_defs.h"
#include "printf.h"

void _start(int argc, char **argv) {
    if (argc != 3) {
        printf("Wrong number of arguments (got %d, expected 3)\n", argc);
        exit();
    }
    char *target = argv[1];
    char *linkpath = argv[2];
    char *path = linkpath;
    for (char *ptr = linkpath; *ptr != 0; ptr++) {
        if (*ptr == '/') {
            path = ptr;
        }
    }
    if (path != linkpath) {
        *path = '\0';
        path++;
    } else {
        linkpath = "";
    }
    int res = link(target, linkpath, path);
    if (res < 0) {
        puts("Link failed\n");
    }
    exit();
}
