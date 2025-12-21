ENTRY(_start)

SECTIONS
{
    /* .multiboot ALIGN(4) : { */
    /*     KEEP(*(.multiboot)) */
    /* } */
    . = 0x00100000;
    .boot ALIGN(16) : {
        KEEP(*(.multiboot))
        *(.boot.text*)
        *(.boot.data*)
    }
    . = 0xC0200000;
    .text ALIGN(16) : AT(ADDR(.text) - 0xC0000000) {
        *(.text*)
    }
    /* . = 0xC0280000; */
    .rodata ALIGN(16) : AT(ADDR(.rodata) - 0xC0000000) {
        *(.rodata*)
    }
    .data ALIGN(4) : AT(ADDR(.data) - 0xC0000000) {
        *(.data*)
    }
    .bss ALIGN(4096) : AT(ADDR(.bss) - 0xC0000000) {
        *(.bss*) *(COMMON)
    }

    /* . = 0xC0000000; */
    /* .higher_half ALIGN(16) : { */
    /*     *(.higher_half*) */
    /* } */
}

