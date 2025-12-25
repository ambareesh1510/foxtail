#include "syscall_defs.h"
#include "malloc.h"
#include "string.h"
#include "printf.h"

bool is_prefix(char *str, char *prefix) {
    unsigned int i = 0;
    while (prefix[i] != '\0') {
        if (str[i] == '\0' || str[i] != prefix[i]) {
            return false;
        }
        i++;
    }
    return true;
}

#define MAX_ARGS 16

int parse_args(char *buf, char *argv[]) {
    int argc = 0;

    while (*buf == ' ') {
        buf++;
    }

    while (*buf != '\0' && argc < MAX_ARGS) {
        argv[argc++] = buf;

        while (*buf && *buf != ' ')
            buf++;

        if (*buf == '\0')
            break;

        *buf = '\0';
        buf++;

        while (*buf == ' ')
            buf++;
    }

    argv[argc] = 0;
    return argc;
}


void _start() {
    char buf[100];
    int bytes;
    for (;;) {
        puts("$ ");
        bytes = read(1, buf, 99);
        if (buf[bytes - 1] == '\n') {
            buf[bytes - 1] = '\0';
        } else {
            buf[bytes] = '\0';
        }

        // Check if cd command
        if (is_prefix(buf, "cd ")) {
            char *dir = buf + 3;
            while (*dir == ' ') {
                dir++;
            }
            int res = cd(dir);
            if (res == 0) {
                puts("Changed directories to ");
                puts(dir);
                puts("\n");
            } else {
                puts("Unable to change directories\n");
            }
        } else if (strcmp(buf, "exit") == 0) {
            exit();
        } else {
            char *argv[MAX_ARGS + 1];
            int argc = parse_args(buf, argv);

            if (argc == 0)
                continue;

            int res = spawn_proc(argv[0], argc, argv);
            if (res < 0) {
                puts("Unable to spawn process ");
                puts(argv[0]);
                puts(".\n");
            } else {
                wait(res);
            }
        }
    }
    exit();
}
