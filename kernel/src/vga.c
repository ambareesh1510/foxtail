#include <stdarg.h>
#include <stdbool.h>
#include "vga.h"
#include "util.h"

void vga_putch_at(char c, u32 row, u32 col, u8 attr) {
    unsigned int idx = row * VGA_WIDTH + col;
    VGA[idx] = ((u16) attr << 8) | (u8) c;
}

void vga_write_at(const char *s, u32 row, u32 col, u8 attr) {
    unsigned int c = col;
    char ch;
    while ((ch = *s++)) {
        vga_putch_at(ch, row, c++, attr);
        if (c >= VGA_WIDTH) {
            c = 0;
            row++;
        }
    }
}

void vga_clear() {
    for (u32 y = 0; y < VGA_HEIGHT; y++) {
        for (u32 x = 0; x < VGA_WIDTH; x++) {
            vga_putch_at(' ', y, x, 0x07);
        }
    }
}

