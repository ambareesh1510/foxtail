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
            int res = spawn_proc(buf);
            if (res < 0) {
                puts("Unable to spawn process ");
                puts(buf);
                puts(".\n");
            } else {
                wait(res);
            }
        }
    }
    exit();
}
