#include <stdarg.h>
#include <stdbool.h>
#include "vga.h"
#include "syscall.h"
#include "syscall_defs.h"
#include "util.h"
#include "kstring.h"

struct graphics_data graphics_data = {};
struct graphics_data *graphics_data_high = (struct graphics_data *) ((u32)(&graphics_data) + HIGHER_HALF_BASE);

u8 vga_font[] = {
#embed "assets/BigRoman-8x16.font"
};
#define VGA_FONT_CHAR_W 8
#define VGA_FONT_CHAR_H 16

u32 vga_width = 80;
u32 vga_height = 25;

void vga_init() {
    if (graphics_data_high->type == VGA_TEXT_MODE) {
        vga_width = 80;
        vga_height = 25;
    } else if (graphics_data_high->type == VGA_GRAPHICS_MODE) {
        vga_width = graphics_data_high->width / VGA_FONT_CHAR_W;
        vga_height = graphics_data_high->height / VGA_FONT_CHAR_H;
        u32 depth = graphics_data_high->depth;
        if (depth != 8 && depth != 16 && depth != 24 && depth != 32) {
            panic("Unsupported bit depth\n");
        }
    } else {
        panic("Unsupported graphics mode\n");
    }
}

void vga_putch_at(char c, u32 row, u32 col, u8 attr) {
    if (graphics_data_high->type == VGA_TEXT_MODE) {
        unsigned int idx = row * vga_width + col;
        VGA[idx] = ((u16) attr << 8) | (u8) c;
    } else if (graphics_data_high->type == VGA_GRAPHICS_MODE) {
        for (u32 dy = 0; dy < VGA_FONT_CHAR_H; dy++) {
            for (u32 dx = 0; dx < VGA_FONT_CHAR_W; dx++) {
                u32 x = col * VGA_FONT_CHAR_W + dx;
                u32 y = row * VGA_FONT_CHAR_H + dy;
                volatile u8 *pixel_ptr = VGA_GRAPHICS_FB + y * graphics_data_high->pitch + x * graphics_data_high->depth / 8;
                u8 byte; 
                if (vga_font[c * VGA_FONT_CHAR_H + dy] & (1 << (VGA_FONT_CHAR_W - dx - 1))) {
                    byte = 0xFF;
                } else {
                    byte = 0x00;
                }
                for (u32 byte_idx = 0; byte_idx < graphics_data_high->depth / 8; byte_idx++) {
                    pixel_ptr[byte_idx] = byte;
                }
            }
        }
    } else {
        panic("Unreachable: unsupported graphics mode\n");
    }
}

void vga_shift_up() {
    if (graphics_data_high->type == VGA_TEXT_MODE) {
        for (u32 y = 0; y < vga_height - 1; y++) {
            for (u32 x = 0; x < vga_width; x++) {
                VGA[y * vga_width + x] = VGA[(y + 1) * vga_width + x];
            }
        }
        for (u32 x = 0; x < vga_width; x++) {
            VGA[(vga_height - 1) * vga_width + x] = 0;
        }
    } else if (graphics_data_high->type == VGA_GRAPHICS_MODE) {
        u32 char_rows_px = VGA_FONT_CHAR_H;

        u8 *fb = (u8 *) VGA_GRAPHICS_FB;

        // Move framebuffer up by one text row
        u32 move_bytes = (graphics_data_high->height - char_rows_px) * graphics_data_high->pitch;
        memmove(
            fb,
            fb + char_rows_px * graphics_data_high->pitch,
            move_bytes
        );

        char *clear_start = (char *) (fb + move_bytes);
        u32 clear_bytes = char_rows_px * graphics_data_high->pitch;
        memset(clear_start, 0x00, clear_bytes);
    } else {
        panic("Unreachable: unsupported graphics mode\n");
    } 
}

void vga_clear() {
    if (graphics_data_high->type == VGA_TEXT_MODE) {
        for (u32 y = 0; y < vga_height; y++) {
            for (u32 x = 0; x < vga_width; x++) {
                vga_putch_at(' ', y, x, 0x07);
            }
        }
    } else {
        for (u32 y = 0; y < graphics_data_high->height; y++) {
            for (u32 x = 0; x < graphics_data_high->width; x++) {
                vga_draw_pixel(0xFF000000, x, y);
            }
        }
    }
}

void write_color8(u32 color, volatile u8 buf[1]) {
    // TODO: palette
    buf[0] = 0xFF;
}

#define A8(c) (((c) >> 24) & 0xFF)
#define R8(c) (((c) >> 16) & 0xFF)
#define G8(c) (((c) >> 8)  & 0xFF)
#define B8(c) (((c) >> 0)  & 0xFF)
void write_color16(u32 color, volatile u8 buf[2]) {
    u16 r = R8(color) >> 3;  // 5 bits
    u16 g = G8(color) >> 2;  // 6 bits
    u16 b = B8(color) >> 3;  // 5 bits

    u16 val = (r << 11) | (g << 5) | b;
    // TODO: check endianness
    buf[1] = (val >> 8) & 0xFF;
    buf[0] = val & 0xFF;
}

void write_color24(u32 color, volatile u8 buf[3]) {
    // TODO: check endianness
    buf[2] = R8(color);
    buf[1] = G8(color);
    buf[0] = B8(color);
}

void write_color32(u32 color, volatile u8 buf[4]) {
    // TODO: check endianness
    buf[3] = A8(color);
    buf[2] = R8(color);
    buf[1] = G8(color);
    buf[0] = B8(color);
}

void vga_draw_pixel(u32 color, u32 x, u32 y) {
    volatile u8 *pixel_ptr = VGA_GRAPHICS_FB + y * graphics_data_high->pitch + x * graphics_data_high->depth / 8;
    if (graphics_data_high->depth == 32) {
        write_color32(color, pixel_ptr);
        // write_color32(0xFFFF0000, pixel_ptr);
    } else if (graphics_data_high->depth == 24) {
        write_color24(color, pixel_ptr);
    } else if (graphics_data_high->depth == 16) {
        write_color16(color, pixel_ptr);
    } else if (graphics_data_high->depth == 8) {
        write_color8(color, pixel_ptr);
    } else {
        panic("Unsupported pixel depth\n");
    }
}
