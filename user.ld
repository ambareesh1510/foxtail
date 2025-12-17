ENTRY(_start)

SECTIONS
{
    . = 0x00400000;
    .text ALIGN(16) : {
        *(.text*)
    }
    .rodata ALIGN(16) : {
        *(.rodata*)
    }
    .data ALIGN(4) : {
        *(.data*)
    }
    .bss ALIGN(4096) : {
        *(.bss*) *(COMMON)
    }
}
