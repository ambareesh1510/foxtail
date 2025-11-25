#include "vga.h"
#include "util.h"

void vga_putch_at(char c, unsigned int row, unsigned int col, uint8_t attr) {
    unsigned int idx = row * VGA_WIDTH + col;
    VGA[idx] = ((uint16_t) attr << 8) | (uint8_t) c;
}

void vga_write_at(const char *s, unsigned int row, unsigned int col, uint8_t attr) {
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
    for (uint32_t y = 0; y < VGA_HEIGHT; y++) {
        for (uint32_t x = 0; x < VGA_WIDTH; x++) {
            vga_putch_at(' ', y, x, 0x07);
        }
    }
}
