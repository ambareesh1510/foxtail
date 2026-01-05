#include "syscall_defs.h"
#include "printf.h"

unsigned int min(unsigned int x, unsigned int y) {
    return (x < y) ? x : y;
}

void _start() {
    puts("Hello from text mode\n");
    int graphics_res = set_graphics_mode(true);
    if (graphics_res == -1) {
        puts("Failed transition to graphics mode\n");
        exit(1);
    }
    set_tty_mode(TTY_MODE_RAW);
    unsigned int w = 1000, h = 1000;
    unsigned int *pixels = malloc(h * w * sizeof(unsigned int));
    for (unsigned int y = 0; y < h; y++) {
        for (unsigned int x = 0; x < w; x++) {
            pixels[y * w + x] = ((x * 256 / w) << 8) + (y * 256 / h);
        }
    }
    draw_pixels(pixels, h, w);
    char *buf = malloc(100);
    for (;;) {
        int bytes = read(1, buf, 100);
        for (int i = 0; i < bytes; i++) {
            if (buf[i] == '\n') {
                goto cleanup;
            }
        }
    }
cleanup:
    set_tty_mode(TTY_MODE_COOKED);
    set_graphics_mode(false);
    exit(0);
}
