/* multiboot.s - minimal multiboot header + entry */
.align 4
.section .multiboot
    MULTIBOOT_MAGIC = 0x1BADB002
    MULTIBOOT_PAGE_ALIGN = 1 << 0
    MULTIBOOT_MEM_INFO = 1 << 1
    MULTIBOOT_GRAPHICS = 1 << 2
    MULTIBOOT_FLAGS = MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEM_INFO | MULTIBOOT_GRAPHICS
    MULTIBOOT_CHECKSUM = -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)
    MULTIBOOT_GRAPHICS_MODE_TYPE = 0
    MULTIBOOT_GRAPHICS_WIDTH = 320
    MULTIBOOT_GRAPHICS_HEIGHT = 200
    MULTIBOOT_GRAPHICS_DEPTH = 8

    .long MULTIBOOT_MAGIC
    .long MULTIBOOT_FLAGS
    .long MULTIBOOT_CHECKSUM
    .fill 6, 4, 0
    // .times 5 .long 0
    .long MULTIBOOT_GRAPHICS_MODE_TYPE
    .long MULTIBOOT_GRAPHICS_WIDTH
    .long MULTIBOOT_GRAPHICS_HEIGHT
    .long MULTIBOOT_GRAPHICS_DEPTH

.section .boot, "ax", %progbits
    .globl _start
    .type _start, @function
_start:
    cli
    call kernel_main

.hang:
    hlt
    jmp .hang

