#include "kprintf.h"
#include "proc.h"
#include "util.h"
#include "interrupt.h"
#include "io.h"
#include "vga.h"

char kb_char;
volatile uint32_t ticks = 0;

struct __attribute__ ((packed)) idt_entry {
    uint16_t offset_low;
    uint16_t segment;
    uint8_t reserved;
    uint8_t flags;
    uint16_t offset_high;
};
_Static_assert(sizeof(struct idt_entry) == 8, "IDT entry must be 8 bytes");

struct __attribute__ ((packed)) idt_descriptor {
    uint16_t size;
    uint32_t offset;
};
_Static_assert(sizeof(struct idt_descriptor) == 6, "IDT descriptor must be 6 bytes");

struct interrupt_frame {
    uint32_t ip;
    uint32_t cs;
    uint32_t flags;
    uint32_t sp;
    uint32_t ss;
};

#define IDT_LENGTH 256

struct idt_entry idt[IDT_LENGTH];

struct idt_descriptor idt_desc;

void idt_write_entry(
    uint32_t index,
    uint32_t offset,
    uint16_t segment,
    uint8_t flags
) {
    idt[index].offset_low = offset & 0xFFFF;
    idt[index].offset_high = (offset >> 16) & 0xFFFF;
    idt[index].segment = segment;
    idt[index].reserved = 0;
    idt[index].flags = flags;
}

__attribute__ ((no_caller_saved_registers))
inline void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        // IRQs 8-15 are handled by the slave PIC
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
}

__attribute__ ((interrupt))
void timer_interrupt_handler(
    struct interrupt_frame *frame
) {
    if (!proc_exists) {
        goto timer_handler_default;
    }
    struct proc *curr_proc = ptable + scheduler_proc_index;
    // Save registers in interrupt frame
    curr_proc->registers.esp = frame->sp;
    curr_proc->registers.eip = frame->ip;
    curr_proc->registers.cs = frame->cs;
    curr_proc->registers.ss = frame->ss;
    curr_proc->registers.eflags = frame->flags;


    // Save general purpose registers
    __asm__ volatile (
        "mov %%eax, %0\n"
        "mov %%ebx, %1\n"
        "mov %%ecx, %2\n"
        "mov %%edx, %3\n"
        "mov %%esi, %4\n"
        "mov %%edi, %5\n"
        "mov %%ebp, %6\n"
        :
        "=m"(curr_proc->registers.eax),
        "=m"(curr_proc->registers.ebx),
        "=m"(curr_proc->registers.ecx),
        "=m"(curr_proc->registers.edx),
        "=m"(curr_proc->registers.esi),
        "=m"(curr_proc->registers.edi),
        "=m"(curr_proc->registers.ebp)
        : : "memory"
    );
    __asm__ volatile ("pushal");
    scheduler();
    __asm__ volatile ("popal");

    // Restore general purpose registers
    __asm__ volatile (
        "mov %0, %%eax\n"
        "mov %1, %%ebx\n"
        "mov %2, %%ecx\n"
        "mov %3, %%edx\n"
        "mov %4, %%esi\n"
        "mov %5, %%edi\n"
        "mov %6, %%ebp\n"
        : :
        "m"(curr_proc->registers.eax),
        "m"(curr_proc->registers.ebx),
        "m"(curr_proc->registers.ecx),
        "m"(curr_proc->registers.edx),
        "m"(curr_proc->registers.esi),
        "m"(curr_proc->registers.edi),
        "m"(curr_proc->registers.ebp)
    );

    curr_proc = ptable + scheduler_proc_index;
    // Save registers in interrupt frame
    frame->sp = curr_proc->registers.esp;
    frame->ip = curr_proc->registers.eip;
    frame->cs = curr_proc->registers.cs;
    frame->ss = curr_proc->registers.ss;
    frame->flags = curr_proc->registers.eflags;

timer_handler_default:
    ticks++;
    pic_send_eoi(0);
}

__attribute__ ((interrupt))
void keyboard_interrupt_handler(
    __attribute__ ((unused)) struct interrupt_frame *frame
) {
    uint8_t scancode = inb(0x60);
    kb_char = scancode;
    pic_send_eoi(1);
}

struct syscall_registers {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
};

__attribute__((naked))
void syscall_interrupt_handler(void) {
    __asm__ volatile(
        "pushal\n"
        // Push the address of the syscall_registers struct that we just
        // constructed on the stack
        "push %esp\n"
        "call syscall_interrupt_handler_inner\n"
        "add $4, %esp\n"
        "popal\n"
        "iret"
    );
}

void syscall_interrupt_handler_inner(struct syscall_registers *s) {
    kprintf("Syscall with eax = %x, ebx = %x, ecx = %x, edx = %x\n", s->eax, s->ebx, s->ecx, s->edx);
}

// * PIC configuration *

// There are two PICs: one is the master and one is the slave. Each PIC can
// handle 8 IRQs. The offset for each PIC controls which IDT entry is used for
// a given IRQ. By default, the master PIC has offset 0x08 and the slave has
// offset 0x70. However, IDT entries 0x00-0x1f are reserved by Intel (for
// software exceptions, etc.). Therefore, we set the offset for the master PIC
// to 0x20 and the offset for the slave to 0x28 to prevent overlap.

#define PIC_MASTER_COMMAND 0x20
#define PIC_MASTER_DATA 0x21
#define PIC_SLAVE_COMMAND 0xA0
#define PIC_SLAVE_DATA 0xA1

#define PIC_MASTER_OFFSET 0x20
#define PIC_SLAVE_OFFSET 0x28

// Command that informs PIC of initialization.
#define PIC_INIT 0x10

// Informs PIC that extra info about the environment will be provided during
// initialization.
#define PIC_ENV 0x01

// Tells the PIC to use 8086 mode.
// TODO: research this!
#define PIC_8086 0x01

void pic_remap() {
    // Send initialization command to both PICs.
    outb(PIC_MASTER_COMMAND, PIC_INIT | PIC_ENV);
    outb(PIC_SLAVE_COMMAND, PIC_INIT | PIC_ENV);

    // Specify that the master PIC should have offset 0x20 and the slave should
    // have offset 0x28.
    outb(PIC_MASTER_DATA, PIC_MASTER_OFFSET);
    outb(PIC_SLAVE_DATA, PIC_SLAVE_OFFSET);

    // Set the cascading mode.
    outb(PIC_MASTER_DATA, 0x04);
    outb(PIC_SLAVE_DATA, 0x02);

    // Set PICS to use 8086 mode.
    outb(PIC_MASTER_DATA, PIC_8086);
    outb(PIC_SLAVE_DATA, PIC_8086);

    // Mask interrupts for all unimplemented IRQs.
    // Currently, only handlers for IRQ0 and IRQ1 are implemented.
    outb(PIC_MASTER_DATA, 0xFC);
    outb(PIC_SLAVE_DATA, 0xFF);
}

#define IDT_FLAG_INTERRUPT_GATE 0x8E

void 
idt_load() {
    idt_desc.size = sizeof(idt) - 1;
    idt_desc.offset = (uint32_t) &idt;

    idt_write_entry(
        0x20,
        (uint32_t) timer_interrupt_handler,
        0x08,
        IDT_FLAG_INTERRUPT_GATE
    );

    idt_write_entry(
        0x21,
        (uint32_t) keyboard_interrupt_handler,
        0x08,
        IDT_FLAG_INTERRUPT_GATE
    );

    idt_write_entry(
        0x80,
        (uint32_t) syscall_interrupt_handler,
        0x08,
        0xEE
    );

    __asm__ volatile ("lidt %0" : : "m" (idt_desc));
    pic_remap();
    __asm__ volatile ("sti");
}
