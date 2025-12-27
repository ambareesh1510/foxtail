#include "syscall_defs.h"
#include "printf.h"

void _start(int argc, char **argv) {
    if (argc != 3) {
        printf("Wrong number of arguments (got %d, expected 2)\n", argc - 1);
        exit(1);
    }
    char *target = argv[1];
    char *linkpath = argv[2];
    char *orig_path = malloc(strlen(linkpath) + 1);
    strcpy(orig_path, linkpath);
    delete(linkpath);
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
    harden(orig_path);
    delete(target);
    exit(0);
}
