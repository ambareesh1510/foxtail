#include "gdt.h"
#include "kprintf.h"
#include "paging.h"
#include "proc.h"
#include "util.h"
#include "interrupt.h"
#include "io.h"
#include "syscall.h"
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

struct __attribute__ ((packed)) interrupt_frame {
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
void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        // IRQs 8-15 are handled by the slave PIC
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
}

// char *a = "TEST_TIMER_HANDLER esp = %d\n";
char *a = "TEST_TIMER_HANDLER %x\n";
__attribute__ ((naked))
void timer_interrupt_handler() {
    __asm__ volatile (
        "cli\n"
        // "push %%esp; push %0\n; call kprintf; add $0x8, %%esp\n"
        "pushal\n"
        "push %%esp\n"
        "call timer_interrupt_handler_inner\n"
        "add $0x4, %%esp\n"
        "popal\n"
        // "push 0(%%esp); push %0\n; call kprintf; add $0x8, %%esp\n"
        "sti\n"
        "iret\n"
        : "=m"(a)
        :
        : "ebp", "esp", "eax", "ebx", "ecx", "edx", "memory"
    );
}

struct __attribute__ ((packed)) regs_and_interrupt_frame {
    struct syscall_registers regs;
    struct interrupt_frame frame;
};

uint32_t global_ra;

void timer_interrupt_handler_inner(
    struct regs_and_interrupt_frame *f
) {
    __asm__ volatile("mov 4(%%ebp), %0" : "=r"(global_ra));
    if (!proc_exists) {
        goto timer_handler_default;
    }
    struct proc *curr_proc = get_current_proc();

    scheduler();

    curr_proc = get_current_proc();

    // Restore cr3
    __asm__ volatile (
        "mov %0, %%eax\n"
        "mov %%eax, %%cr3\n"
        : : "m"(curr_proc->cr3) : "eax"
    );
    
    if (curr_proc->status != EMBRYO) {
        goto timer_handler_default;
    }

    curr_proc = get_current_proc();

    f = (struct regs_and_interrupt_frame *) (HIGHER_HALF_BASE - PGSIZE - sizeof(*f));
    curr_proc->status = RUNNABLE;

    // Restore registers in interrupt frame
    f->frame.sp = curr_proc->registers.esp;
    f->frame.ip = curr_proc->registers.eip;
    f->frame.cs = curr_proc->registers.cs;
    f->frame.ss = curr_proc->registers.ss;
    f->frame.flags = curr_proc->registers.eflags;

    // Restore general purpose registers
    f->regs.eax = curr_proc->registers.eax;
    f->regs.ebx = curr_proc->registers.ebx;
    f->regs.ecx = curr_proc->registers.ecx;
    f->regs.edx = curr_proc->registers.edx;
    f->regs.esi = curr_proc->registers.esi;
    f->regs.edi = curr_proc->registers.edi;
    f->regs.ebp = curr_proc->registers.ebp;

    tss.esp0 = HIGHER_HALF_BASE - PGSIZE;

    __asm__ volatile ("movl %0, 0x4(%%ebp)" : : "r"(global_ra) : "memory");
timer_handler_default:
    ticks++;
    pic_send_eoi(0);
}

// Source - https://stackoverflow.com/a
// Posted by jonathan
// Retrieved 2025-12-17, License - CC BY-SA 4.0

char kbd_US [128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',   
  '\t', /* <-- Tab */
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',     
    0, /* <-- control key */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',  0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,
  '*',
    0,  /* Alt */
  ' ',  /* Space bar */
    0,  /* Caps lock */
    0,  /* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,  /* < ... F10 */
    0,  /* 69 - Num lock*/
    0,  /* Scroll Lock */
    0,  /* Home key */
    0,  /* Up Arrow */
    0,  /* Page Up */
  '-',
    0,  /* Left Arrow */
    0,
    0,  /* Right Arrow */
  '+',
    0,  /* 79 - End key*/
    0,  /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert Key */
    0,  /* Delete Key */
    0,   0,   0,
    0,  /* F11 Key */
    0,  /* F12 Key */
    0,  /* All other keys are undefined */
};


char input_buffer[INPUT_BUFFER_LEN] = {0};
uint32_t input_buffer_write_ptr = 0;
uint32_t input_buffer_read_ptr = 0;
bool input_buffer_nonempty = false;

__attribute__ ((interrupt))
void keyboard_interrupt_handler(
    __attribute__ ((unused)) struct interrupt_frame *frame
) {
    uint8_t scancode = inb(0x60);
    kb_char = scancode;
    if (kbd_US[scancode] != 0) {
        char c = kbd_US[scancode];
        input_buffer[input_buffer_write_ptr] = c;
        __asm__ volatile ("pushal");
        kprint_char(c);
        __asm__ volatile ("popal");
        input_buffer_write_ptr = (input_buffer_write_ptr + 1) % INPUT_BUFFER_LEN;
        input_buffer_nonempty = true;
    }
    pic_send_eoi(1);
}

__attribute__((naked))
void syscall_interrupt_handler(void) {
  __asm__ volatile(
      // TODO: we disable interrupts during syscalls... this is BAD! figure out
      // how to allow nested interrupts (probably use a separate kernel stack
      // per process -- this should be an easy fix)
      "cli\n"
      "pushal\n"
      // Push the address of the syscall_registers struct that we just
      // constructed on the stack
      "push %esp\n"
      "call syscall_interrupt_handler_inner\n"
      "add $4, %esp\n"
      "popal\n"
      "sti\n"
      "iret");
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
