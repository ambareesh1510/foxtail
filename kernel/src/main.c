/* main.c - minimal kernel printing to VGA text mode */
typedef unsigned long size_t;
typedef unsigned int  uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

// called from assembly
void kernel_main(void);

volatile uint16_t * const VGA = (volatile uint16_t *) 0xB8000;
const uint32_t VGA_W = 80;

// write char to VGA row, col with attr
static void vga_putch_at(char c, unsigned int row, unsigned int col, uint8_t attr) {
    unsigned int idx = row * VGA_W + col;
    VGA[idx] = ((uint16_t) attr << 8) | (uint8_t) c;
}

// write null-terminated string at VGA row, col
static void vga_write_at(const char *s, unsigned int row, unsigned int col, uint8_t attr) {
    unsigned int c = col;
    char ch;
    while ((ch = *s++)) {
        vga_putch_at(ch, row, c++, attr);
        if (c >= VGA_W) {
            c = 0;
            row++;
        }
    }
}

// I/O helpers
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

uint8_t keyboard_get_scancode(void) {
    while ((inb(0x64) & 1) == 0)
        ;
    return inb(0x60);
}

void kernel_main(void) {
    const char *msg = "Hello from 32-bit protected mode (GRUB) - VGA text!";
    vga_write_at(msg, 10, 10, 0x07); /* normal white-on-black */

    /* show some numbers to verify 32-bit operations */
    const char *nums = "Numbers: 1234567890";
    vga_write_at(nums, 12, 10, 0x07);

    uint8_t sc = keyboard_get_scancode();
    char buf[32];
    if (sc == 0x1E) {
        vga_write_at("Pressed A", 7, 5, 0x07);
    } else {
        vga_write_at("Pressed something else", 7, 5, 0x07);
    }


    /* loop forever (halt CPU to reduce host load) */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

