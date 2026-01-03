#ifndef VGA_H
#define VGA_H

#include "util.h"
#include "paging.h"

#define VGA ((volatile u16 *) (0xB8000 + HIGHER_HALF_BASE))
#define VGA_GRAPHICS_FB ((volatile u8 *) (0xD0000000))

extern u32 vga_width;
extern u32 vga_height;

struct graphics_mode {
    enum {
        VGA_TEXT_MODE,
        VGA_GRAPHICS_MODE
    } type;
    u32 width;
    u32 height;
    u32 depth;
    u32 pitch;
    u32 framebuffer;
};

__attribute__ ((section(".boot.data")))
extern struct graphics_mode graphics_mode;
extern struct graphics_mode *graphics_mode_high;

void vga_init();

// write char to VGA row, col with attr
void vga_putch_at(char c, u32 row, u32 col, u8 attr);
void vga_shift_up();

void vga_clear();

#endif /* VGA_H */
