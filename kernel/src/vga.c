#include <stdarg.h>
#include <stdbool.h>
#include "vga.h"
#include "util.h"
#include "kstring.h"

struct graphics_mode graphics_mode = {};
struct graphics_mode *graphics_mode_high = (struct graphics_mode *) ((u32)(&graphics_mode) + HIGHER_HALF_BASE);

u8 vga_font[] = {
#embed "assets/BigRoman-8x16.font"
};
#define VGA_FONT_CHAR_W 8
#define VGA_FONT_CHAR_H 16

u32 vga_width = 80;
u32 vga_height = 25;

void vga_init() {
    if (graphics_mode_high->type == VGA_TEXT_MODE) {
        vga_width = 80;
        vga_height = 25;
    } else if (graphics_mode_high->type == VGA_GRAPHICS_MODE) {
        vga_width = graphics_mode_high->width / VGA_FONT_CHAR_W;
        vga_height = graphics_mode_high->height / VGA_FONT_CHAR_H;
    } else {
        panic("Unsupported graphics mode\n");
    }
}

void vga_putch_at(char c, u32 row, u32 col, u8 attr) {
    if (graphics_mode_high->type == VGA_TEXT_MODE) {
        unsigned int idx = row * vga_width + col;
        VGA[idx] = ((u16) attr << 8) | (u8) c;
    } else if (graphics_mode_high->type == VGA_GRAPHICS_MODE) {
        for (u32 dy = 0; dy < VGA_FONT_CHAR_H; dy++) {
            for (u32 dx = 0; dx < VGA_FONT_CHAR_W; dx++) {
                u32 x = col * VGA_FONT_CHAR_W + dx;
                u32 y = row * VGA_FONT_CHAR_H + dy;
                volatile u8 *pixel_ptr = VGA_GRAPHICS_FB + y * graphics_mode_high->pitch + x * graphics_mode_high->depth / 8;
                u8 byte; 
                if (vga_font[c * VGA_FONT_CHAR_H + dy] & (1 << (VGA_FONT_CHAR_W - dx - 1))) {
                    byte = 0xFF;
                } else {
                    byte = 0x00;
                }
                for (u32 byte_idx = 0; byte_idx < graphics_mode_high->depth / 8; byte_idx++) {
                    pixel_ptr[byte_idx] = byte;
                }
            }
        }
    } else {
        panic("Unreachable: unsupported graphics mode\n");
    }
}

void vga_shift_up() {
    if (graphics_mode_high->type == VGA_TEXT_MODE) {
        for (u32 y = 0; y < vga_height - 1; y++) {
            for (u32 x = 0; x < vga_width; x++) {
                VGA[y * vga_width + x] = VGA[(y + 1) * vga_width + x];
            }
        }
        for (u32 x = 0; x < vga_width; x++) {
            VGA[(vga_height - 1) * vga_width + x] = 0;
        }
    } else if (graphics_mode_high->type == VGA_GRAPHICS_MODE) {
        u32 char_rows_px = VGA_FONT_CHAR_H;

        u8 *fb = (u8 *) VGA_GRAPHICS_FB;

        // Move framebuffer up by one text row
        u32 move_bytes = (graphics_mode_high->height - char_rows_px) * graphics_mode_high->pitch;
        memmove(
            fb,
            fb + char_rows_px * graphics_mode_high->pitch,
            move_bytes
        );

        char *clear_start = (char *) (fb + move_bytes);
        u32 clear_bytes = char_rows_px * graphics_mode_high->pitch;
        memset(clear_start, 0x00, clear_bytes);
    } else {
        panic("Unreachable: unsupported graphics mode\n");
    } 
}

void vga_clear() {
    for (u32 y = 0; y < vga_height; y++) {
        for (u32 x = 0; x < vga_width; x++) {
            vga_putch_at(' ', y, x, 0x07);
        }
    }
}

