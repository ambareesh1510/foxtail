#include "string.h"

void _start(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i != 1) {
            puts(" ");
        }
        puts(argv[i]);
    }
    puts("\n");
    exit(0);
}
