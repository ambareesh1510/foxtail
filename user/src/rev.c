#include "syscall_defs.h"
#include "printf.h"
#include "string.h"

#define BUF_SIZE 256

// reverse s[l..r-1]
static void reverse_range(char *s, int l, int r) {
    r--; // exclusive -> inclusive
    while (l < r) {
        char tmp = s[l];
        s[l] = s[r];
        s[r] = tmp;
        l++;
        r--;
    }
}

void _start() {
    char buf[BUF_SIZE];
    char line[BUF_SIZE];
    int line_len = 0;

    while (1) {
        int n = read(1, buf, BUF_SIZE);
        if (n <= 0)
            break;

        for (int i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\n') {
                // reverse current line and print
                reverse_range(line, 0, line_len);
                write(0, line, line_len);
                write(0, "\n", 1);
                line_len = 0;
            } else {
                if (line_len < BUF_SIZE - 1) {
                    line[line_len++] = c;
                }
            }
        }
    }

    // Handle last line if no trailing newline
    if (line_len > 0) {
        reverse_range(line, 0, line_len);
        write(0, line, line_len);
        write(0, "\n", 1);
    }

    exit(0);
}

