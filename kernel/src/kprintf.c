#include "kprintf.h"

#include <stdarg.h>
#include <stdbool.h>
#include "vga.h"
#include "util.h"

u32 cursor_row = 0, cursor_column = 0;

void kprint_backspace() {
    if (cursor_column == 0) {
        if (cursor_row == 0) {
            return;
        }
        cursor_row--;
        cursor_column = vga_width - 1;
    } else {
        cursor_column--;
    }
    kprint_char(' ');
    if (cursor_column == 0) {
        cursor_row--;
        cursor_column = vga_width - 1;
    } else {
        cursor_column--;
    }
}

void kprint_next_line() {
    cursor_column = 0;
    if (cursor_row == vga_height - 1) {
        vga_shift_up();
    } else {
        cursor_row++;
    }
}

void kprint_uint32_hex(u32 c, char *buf) {
    const char hex_digits[] = "0123456789ABCDEF";
    u32 len = 0;
    for (u32 i = 4; i < 32; i += 4) {
        if (((c >> i) & 0xF) != 0) {
            len = i;
        }
    }
    for (u32 i = 0; i <= len; i += 4) {
        buf[i / 4] = hex_digits[(c >> (len - i)) & 0xF];
    }
    buf[1 + len / 4] = '\0';
}

void kprint_uint32_dec(u32 value, char* buf) {
    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    int i = 0;
    // Find the digits in reverse order.
    while (value > 0) {
        buf[i] = '0' + (value % 10);
        value /= 10;
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

void kprint_char(char c) {
    if (c == '\n') {
        kprint_next_line();
    } else if (c == '\b') {
        kprint_backspace();
    } else {
        vga_putch_at(c, cursor_row, cursor_column, 0x07);
        cursor_column++;
        if (cursor_column >= vga_width) {
            kprint_next_line();
        }
    }
}

void kprint(const char *str) {
    while (*str != 0) {
        kprint_char(*str);
        str++;
    }
}

void vkprintf(const char *fmt, va_list args) {
    const char *curr = fmt;
    bool format_spec = false;
    while (*curr != 0) {
        if (format_spec) {
            if (*curr == 'd') {
                u32 val = va_arg(args, u32);
                char buf[11];
                kprint_uint32_dec(val, buf);
                kprint(buf);
            } else if (*curr == 'x') {
                u32 val = va_arg(args, u32);
                char buf[9];
                kprint_uint32_hex(val, buf);
                kprint(buf);
            } else if (*curr == 's') {
                char *val = va_arg(args, char *);
                kprint(val);
            }
            format_spec = false;
        } else if (*curr == '%') {
            format_spec = true;
        } else if (*curr == '\b') {
            kprint_backspace();
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
            if (cursor_column >= vga_width) {
                kprint_next_line();
            }
        }
        curr++;
    }
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vkprintf(fmt, args);
    va_end(args);
}

void move_cursor(u32 row, u32 col) {
    cursor_row = row;
    cursor_column = col;
}
