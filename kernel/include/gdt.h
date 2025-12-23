#ifndef GDT_H
#define GDT_H

#include "util.h"

struct __attribute__ ((packed)) tss_entry {
    u32 prev_tss;
    u32 esp0;
    u32 ss0;
    u32 esp1;
    u32 ss1;
    u32 esp2;
    u32 ss2;
    u32 cr3;
    u32 eip;
    u32 eflags;
    u32 eax, ecx, edx, ebx, esp, ebp, esi, edi;
    u32 es, cs, ss, ds, fs, gs;
    u32 ldt;
    u16 trap;
    u16 iomap_base;
};

extern struct tss_entry tss;

void gdt_load();

#endif /* GDT_H */
