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

bool contains_pipe(const char *s) {
    while (*s) {
        if (*s == '|')
            return true;
        s++;
    }
    return false;
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
    int exit_code = 0;
    for (;;) {
        if (exit_code == 0) {
            puts("$> ");
        } else {
            puts("!> ");
        }
        bytes = read(1, buf, 99);
        if (bytes <= 0) {
            break;
        }
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
            exit(0);
        } else if (contains_pipe(buf)) {
            // split into left and right commands
            char *pipe_pos = strchr(buf, '|');
            *pipe_pos = '\0';

            char *left = buf;
            char *right = pipe_pos + 1;

            trim(left);
            trim(right);

            struct pipe p;
            if (pipe(&p) < 0) {
                puts("pipe failed\n");
                continue;
            }

            // ---- left command ----
            char *argv_left[MAX_ARGS + 1];
            unsigned int argc_left = parse_args(left, argv_left);

            struct spawn_custom_command left_cmd;
            left_cmd.type = SPAWN_CUSTOM_COMMAND_REMAP_FDS;
            left_cmd.data.remap_fds.curr_fd = p.write_fd;
            left_cmd.data.remap_fds.new_fd = 0;

            int left_pid = spawn_proc(
                argv_left[0],
                argc_left,
                argv_left,
                1,
                &left_cmd
            );

            if (left_pid < 0) {
                puts("spawn left failed\n");
                continue;
            }

            // ---- right command ----
            char *argv_right[MAX_ARGS + 1];
            unsigned int argc_right = parse_args(right, argv_right);

            struct spawn_custom_command right_cmd;
            right_cmd.type = SPAWN_CUSTOM_COMMAND_REMAP_FDS;
            right_cmd.data.remap_fds.curr_fd = p.read_fd;
            right_cmd.data.remap_fds.new_fd = 1;

            int right_pid = spawn_proc(
                argv_right[0],
                argc_right,
                argv_right,
                1,
                &right_cmd
            );

            // TODO: we can't just continue here (resource leakage); fix this
            if (right_pid < 0) {
                puts("spawn right failed\n");
                continue;
            }

            // Shell must close its own copy of pipe ends
            close(p.read_fd);
            close(p.write_fd);

            exit_code = wait(left_pid);
            exit_code = wait(right_pid);
        } else {
            char *argv[MAX_ARGS + 1];
            int argc = parse_args(buf, argv);

            if (argc == 0)
                continue;

            int res = spawn_proc(argv[0], argc, argv, 0, 0);
            if (res < 0) {
                puts("Unable to spawn process ");
                puts(argv[0]);
                puts(".\n");
            } else {
                exit_code = wait(res);
            }
        }
    }
    exit(0);
}
