#include "kprintf.h"

#include <stdarg.h>
#include <stdbool.h>
#include "vga.h"
#include "util.h"

uint32_t cursor_row, cursor_column;

void kprint_next_line() {
    cursor_column = 0;
    if (cursor_row == VGA_HEIGHT - 1) {
        for (uint32_t y = 0; y < VGA_HEIGHT - 1; y++) {
            for (uint32_t x = 0; x < VGA_WIDTH; x++) {
                VGA[y * VGA_WIDTH + x] = VGA[(y + 1) * VGA_WIDTH + x];
            }
        }
    } else {
        cursor_row++;
    }
}

void kprint_uint32_hex(uint32_t c, char *buf) {
    const char hex_digits[] = "0123456789ABCDEF";
    uint32_t len = 0;
    for (uint32_t i = 4; i < 32; i += 4) {
        if (((c >> i) & 0xF) != 0) {
            len = i;
        }
    }
    for (uint32_t i = 0; i <= len; i += 4) {
        buf[i / 4] = hex_digits[(c >> (len - i)) & 0xF];
    }
    buf[1 + len / 4] = '\0';
}

void kprint_uint32_dec(uint32_t value, char* buf) {
    // Handle the special case of 0.
    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    int i = 0;
    // Find the digits in reverse order.
    while (value > 0) {
        buf[i] = '0' + (value % 10);  // Get the last digit
        value /= 10;                   // Remove the last digit
        i++;
    }

    // Null-terminate the string
    buf[i] = '\0';

    // Reverse the string to get the correct order
    int start = 0;
    int end = i - 1;
    while (start < end) {
        // Swap the characters
        char temp = buf[start];
        buf[start] = buf[end];
        buf[end] = temp;
        start++;
        end--;
    }
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    const char *curr = fmt;
    bool format_spec = false;
    while (*curr != 0) {
        if (format_spec) {
            if (*curr == 'd') {
                uint32_t val = va_arg(args, uint32_t);
                char buf[11];
                kprint_uint32_dec(val, buf);
                kprintf(buf);
            } else if (*curr == 'x') {
                uint32_t val = va_arg(args, uint32_t);
                char buf[9];
                kprint_uint32_hex(val, buf);
                kprintf(buf);
            } else if (*curr == 's') {
                char *val = va_arg(args, char *);
                kprintf(val);
            }
            format_spec = false;
        } else if (*curr == '%') {
            format_spec = true;
        } else if (*curr == '\n') {
            kprint_next_line();
        } else {
            vga_putch_at(
                *curr,
                cursor_row,
                cursor_column,
                0x07
            );
            cursor_column++;
            if (cursor_column >= VGA_WIDTH) {
                kprint_next_line();
            }
        }
        curr++;
    }
    va_end(args);
}
