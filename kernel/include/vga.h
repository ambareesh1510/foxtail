#ifndef VGA_H
#define VGA_H

#include "util.h"
#include "paging.h"

#define VGA ((volatile uint16_t *) (0xB8000 + HIGHER_HALF_BASE))
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// write char to VGA row, col with attr
void vga_putch_at(char c, uint32_t row, uint32_t col, uint8_t attr);

// write null-terminated string at VGA row, col
void vga_write_at(const char *s, uint32_t row, uint32_t col, uint8_t attr);

void vga_clear();

#endif /* VGA_H */
