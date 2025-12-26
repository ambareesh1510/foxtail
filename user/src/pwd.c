#include "syscall_defs.h"
#include "string.h"
#include "malloc.h"

void _start() {
    int len = 100;
    char *buf = malloc(len);
    int res;
    while ((res = pwd(buf, len)) < 0) {
        len *= 2;
        buf = realloc(buf, len);
    }
    puts(buf);
    puts("\n");
    exit(0);
}
