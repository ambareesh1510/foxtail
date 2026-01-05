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
    if (is_prefix(filename, "~") || is_prefix(filename, ".")) {
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

int process_cmd(char *buf) {
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
            return 0;
        } else {
            puts("Unable to change directories\n");
            return -1;
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
            return -1;
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
            return -1;
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
            return -1;
        }

        // Shell must close its own copy of pipe ends
        close(p.read_fd);
        close(p.write_fd);

        set_tty_mode(TTY_MODE_COOKED);
        wait(left_pid);
        int wait_res = wait(right_pid);
        set_tty_mode(TTY_MODE_RAW);
        return wait_res;
    } else {
        char *argv[MAX_ARGS + 1];
        int argc = parse_args(buf, argv);

        if (argc == 0) {
            return -1;
        }

        // puts("Spawning with:\n");
        // for (int i = 0; i < argc; i++) {
        //     printf("argv[%d] = %s\n", i, argv[i]);
        // }
        //
        int res = spawn_with_path(env_path, argv[0], argc, argv, 0, 0);
        if (res < 0) {
            puts("Unable to spawn process ");
            puts(argv[0]);
            puts(".\n");
            return -1;
        } else {
            set_tty_mode(TTY_MODE_COOKED);
            int wait_res = wait(res);
            set_tty_mode(TTY_MODE_RAW);
            return wait_res;
        }
    }
}

void _start() {
    enum tty_mode old_tty_mode = get_tty_mode();
    set_tty_mode(TTY_MODE_RAW);
    char *line = 0;
    int line_len = 0;
    int line_cap = 0;

    int exit_code = 0;
    bool is_tty = (ftype(1) == SYS_FTYPE_TTY);
    for (;;) {
        if (is_tty) {
            if (exit_code == 0) {
                puts("$> ");
            } else {
                puts("!> ");
            }
        }
        for (;;) {
            char buf[100];
            int bytes;
            bytes = read(1, buf, sizeof(buf));
            if (bytes <= 0) {
                exit(0);
            }
            for (int i = 0; i < bytes; i++) {
                char c = buf[i];

                if (c == '\n') {
                    if (is_tty) {
                        puts("\n");
                    }
                    if (line_cap == line_len) {
                        char *new_line = realloc(line, line_len + 1);
                        if (!new_line) goto oom;
                        line = new_line;
                        line_cap = line_len + 1;
                    }
                    line[line_len] = '\0';

                    exit_code = process_cmd(line);
                    line_len = 0;
                    goto end_cmd;
                } else if (c == '\b' || c == 127) {
                    if (line_len > 0) {
                        line_len--;
                        if (is_tty) {
                            puts("\b");
                        }
                    }
                } else {
                    if (line_len + 1 >= line_cap) {
                        int new_cap = line_cap ? line_cap * 2 : 64;
                        // TODO: This while should never execute (?)
                        while (new_cap <= line_len + 1) {
                            new_cap *= 2;
                        }

                        char *new_line = realloc(line, new_cap);
                        if (!new_line) goto oom;
                        line = new_line;
                        line_cap = new_cap;
                    }

                    line[line_len++] = c;
                    if (is_tty) {
                        char tmp[] = {c, 0};
                        puts(tmp);
                    }
                }

            }
        }
end_cmd:
    }
oom:
    puts("Shell out of memory\n");
    free(line);
    set_tty_mode(old_tty_mode);
    exit(0);
}
