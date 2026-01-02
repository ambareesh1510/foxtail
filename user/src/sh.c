#include "syscall_defs.h"
#include "malloc.h"
#include "string.h"
#include "printf.h"

bool is_prefix(const char *str, const char *prefix) {
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


char *env_path = "~/bin";

int spawn_with_path(
    const char *env_path,
    const char *filename,
    unsigned int argc,
    char **argv,
    unsigned int num_custom_commands,
    struct spawn_custom_command *commands
) {
    // If filename is absolute, don't process PATH
    if (is_prefix(filename, "~")) {
        return spawn_proc(filename, argc, argv, num_custom_commands, commands);
    }
    
    // If path is null, just use pwd
    if (!env_path || env_path[0] == '\0') {
        return spawn_proc(filename, argc, argv, num_custom_commands, commands);
    }
    
    
    // Try each directory in PATH
    int dir_start = 0;
    while (1) {
        // Find next ':' or end of string
        int dir_end = dir_start;
        while (env_path[dir_end] && env_path[dir_end] != ':') {
            dir_end++;
        }
        
        int dir_len = dir_end - dir_start;
        if (dir_len == 0) {
            // Empty component, skip
            if (!env_path[dir_end]) break;
            dir_start = dir_end + 1;
            continue;
        }

        int total_len = dir_len + 1 + strlen(filename) + 1;
        char *full_path = malloc(total_len);
        if (full_path == 0) {
            return -1;
        }
        // Build full path: dir + "/" + filename
        int i;
        for (i = 0; i < dir_len; i++) {
            full_path[i] = env_path[dir_start + i];
        }
        full_path[i] = '\0';
        
        // Add '/'
        full_path[i++] = '/';
        full_path[i] = '\0';
        
        strcat(full_path, filename);
        
        // Try to spawn
        int pid = spawn_proc(full_path, argc, argv, num_custom_commands, commands);

        free(full_path);
        
        if (pid >= 0) {
            return pid;
        }
        
        if (!env_path[dir_end]) {
            break;
        }
        // Next dir in PATH
        dir_start = dir_end + 1;
    }
    
    // All paths failed
    return -1;
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

            int left_pid = spawn_with_path(
                env_path,
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

            int right_pid = spawn_with_path(
                env_path,
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

            int res = spawn_with_path(env_path, argv[0], argc, argv, 0, 0);
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
