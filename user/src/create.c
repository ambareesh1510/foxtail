#include "syscall_defs.h"
#include "printf.h"

void _start(int argc, char **argv) {
    if (argc < 2) {
        puts("Expected at least one argument\n");
        exit(1);
    }
    for (int i = 1; i < argc; i++) {
        char *dirname = argv[i];
        int len = strlen(dirname);
        bool dir = false;
        if (dirname[len - 1] == '/') {
            dir = true;
            dirname[len - 1] = '\0';
        }
        char *filename = dirname;
        for (char *ptr = dirname; *ptr != '\0'; ptr++) {
            if (*ptr == '/') {
                filename = ptr;
            }
        }
        if (filename != dirname) {
            *filename = '\0';
        } else {
            dirname = ".";
        }
        int mode;
        if (dir) {
            mode = SYS_CREATE_DIR;
        } else {
            mode = SYS_CREATE_FILE;
        }
        int res = create(dirname, filename, mode);
        if (res < 0) {
            printf("Couldn't create %s/%s\n", dirname, filename);
        }
    }
    exit(0);
}
