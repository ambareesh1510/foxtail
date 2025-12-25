#include "string.h"

void _start(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        puts(argv[i]);
        puts(" ");
    }
    puts("\n");
    exit();
}
