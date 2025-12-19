#include "gdt.h"
#include "paging.h"
#include "util.h"

typedef uint8_t gdt_entry[8];

#define GDT_LENGTH 6

gdt_entry gdt[GDT_LENGTH];

void gdt_write_entry(
    uint32_t index,
    // 32-bit base
    uint32_t base,
    // 20-bit limit
    uint32_t limit,
    // 8-bit access byte
    uint8_t access_byte,
    // 4-bit flags
    uint8_t flags
) {
    gdt[index][0] = limit & 0xFF;
    gdt[index][1] = (limit >> 8) & 0xFF;
    gdt[index][6] = (limit >> 16) & 0x0F;

    gdt[index][2] = base & 0xFF;
    gdt[index][3] = (base >> 8) & 0xFF;
    gdt[index][4] = (base >> 16) & 0xFF;
    gdt[index][7] = (base >> 24) & 0xFF;
    
    // Encode the access byte
    gdt[index][5] = access_byte;
    
    // Encode the flags
    gdt[index][6] |= (flags << 4);
}

struct __attribute__ ((packed)) gdt_descriptor {
    uint16_t size;
    uint32_t offset;
};

struct gdt_descriptor gdt_desc;

struct tss_entry tss = {0};

char tss_kernel_stack[PGSIZE];

void
gdt_load() {
    __asm__ volatile ("cli");

    gdt_desc.size = sizeof(gdt) - 1;
    gdt_desc.offset = (uint32_t) &gdt;

    gdt_write_entry(0, 0, 0, 0, 0);
    gdt_write_entry(1, 0, 0xFFFFF, 0x9A, 0xC);
    gdt_write_entry(2, 0, 0xFFFFF, 0x92, 0xC);
    gdt_write_entry(3, 0, 0xFFFFF, 0xFA, 0xC);
    gdt_write_entry(4, 0, 0xFFFFF, 0xF2, 0xC);

    tss.ss0 = 0x10;
    // Don't set esp0, since it'll be set in exec/sched
    // tss.esp0 = (uint32_t) (tss_kernel_stack + PGSIZE);

    gdt_write_entry(5, (uint32_t) (&tss), sizeof(tss) - 1, 0x89, 0);

    __asm__ volatile ("lgdt %0" : : "m" (gdt_desc));

    __asm__ volatile (
        "ljmp $0x08, $flush\n"
        "flush:\n"
        "nop\n"
        "mov $0x10, %ax\n"
        "mov %ax, %ds\n"
        "mov %ax, %es\n"
        "mov %ax, %fs\n"
        "mov %ax, %gs\n"
        "mov %ax, %ss\n"
    );
    __asm__ volatile("ltr %%ax" : : "a"(0x28));
}
