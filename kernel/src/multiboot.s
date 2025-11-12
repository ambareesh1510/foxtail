/* multiboot.s - minimal multiboot header + entry */
.align 4
.section .multiboot
    MULTIBOOT_MAGIC = 0x1BADB002
    MULTIBOOT_PAGE_ALIGN = 1 << 0
    MULTIBOOT_MEM_INFO = 1 << 1
    MULTIBOOT_FLAGS = MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEM_INFO
    MULTIBOOT_CHECKSUM = -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

    .long MULTIBOOT_MAGIC
    .long MULTIBOOT_FLAGS
    .long MULTIBOOT_CHECKSUM

    .section .text
    .globl _start
    .type _start, @function
_start:
    /* Clear interrupts (GRUB probably already set things up)
       and call kernel_main (C). */
    cli
    call kernel_main

.hang:
    hlt
    jmp .hang

