#include "cleanup.h"
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
volatile u32 ticks = 0;

struct __attribute__ ((packed)) idt_entry {
    u16 offset_low;
    u16 segment;
    u8 reserved;
    u8 flags;
    u16 offset_high;
};
_Static_assert(sizeof(struct idt_entry) == 8, "IDT entry must be 8 bytes");

struct __attribute__ ((packed)) idt_descriptor {
    u16 size;
    u32 offset;
};
_Static_assert(sizeof(struct idt_descriptor) == 6, "IDT descriptor must be 6 bytes");

struct __attribute__ ((packed)) interrupt_frame {
    u32 ip;
    u32 cs;
    u32 flags;
    u32 sp;
    u32 ss;
};

#define IDT_LENGTH 256

struct idt_entry idt[IDT_LENGTH];

struct idt_descriptor idt_desc;

void idt_write_entry(
    u32 index,
    u32 offset,
    u16 segment,
    u8 flags
) {
    idt[index].offset_low = offset & 0xFFFF;
    idt[index].offset_high = (offset >> 16) & 0xFFFF;
    idt[index].segment = segment;
    idt[index].reserved = 0;
    idt[index].flags = flags;
}

void pic_send_eoi(u8 irq) {
    if (irq >= 8) {
        // IRQs 8-15 are handled by the slave PIC
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
}

struct fault_frame_with_error_code {
    u32 error_code;
    u32 ip;
    u32 cs;
    u32 eflags;
};

__attribute__ ((naked))
void gp_fault_handler() {
    __asm__ volatile (
        "push %esp\n"
        "call gp_fault_handler_inner\n"
        "add $0x8, %esp\n"
        "iret\n"
    );
}

void gp_fault_handler_inner(struct fault_frame_with_error_code *f) {
    if ((f->cs & 0x3) == 3) {
        struct proc *curr = get_current_proc();
        cleanup_proc(curr);
        kprintf("Killed process `%s` (PID %d): General Protection Fault\n", curr->name, curr->pid);
        __asm__ volatile("int $0x20");
    } else {
        panic(
            "General protection fault in kernel mode!\n"
            "Error code: %x (EIP=0x%x CS = 0x%x)\n",
            f->ip,
            f->cs,
            f->error_code
        );
    }
    return;
}

__attribute__ ((naked))
void page_fault_handler() {
    __asm__ volatile (
        "push %esp\n"
        "call page_fault_handler_inner\n"
        "add $0x8, %esp\n"
        "iret\n"
    );
}

void page_fault_handler_inner(struct fault_frame_with_error_code *f) {
    u32 fault_addr;
    __asm__ volatile (
        "mov %%cr2, %0\n"
        : "=r"(fault_addr)
        : : "memory"
    );
    if ((f->error_code & 0x4) || (fault_addr < HIGHER_HALF_BASE)) {
        struct proc *curr = get_current_proc();
        cleanup_proc(curr);
        kprintf("Killed process `%s` (PID %d): Page Fault at address 0x%x (eip=%x)\n", curr->name, curr->pid, fault_addr, f->ip);
        __asm__ volatile("int $0x20");
    } else {
        panic(
            "Page fault in kernel mode! (address 0x%x)\n"
            "Error code: %x (P=%d W=%d U=%d)\n",
            fault_addr,
            f->error_code,
            f->error_code & 1,
            (f->error_code >> 1) & 1,
            (f->error_code >> 2) & 1
        );
    }
    return;
}

struct __attribute__ ((packed)) regs_and_interrupt_frame {
    struct syscall_registers regs;
    struct interrupt_frame frame;
};

u32 get_curr_proc_status() {
    return get_current_proc()->status;
}

void set_curr_proc_runnable() {
    struct proc *curr_proc = get_current_proc();
    curr_proc->status = RUNNABLE;
}

u32 get_curr_pid() {
    return get_current_proc()->pid;
}

__attribute__ ((naked))
void timer_interrupt_handler() {
    __asm__ volatile (
        "pushal\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "cld\n"
        "call timer_interrupt_handler_inner\n"
        // If it's an embryo, set the esp to
        // HIGHER_HALF_BASE - PGSIZE - sizeof(struct regs_and_interrupt_frame)
        "cmp %0, %%eax\n"
        "jne not_embryo\n"
        "mov %1, %%esp\n"
        "call set_curr_proc_runnable\n"
        "not_embryo:\n"
        "call get_curr_pid\n"
        "test %%eax, %%eax\n"
        "je timer_idle_proc\n"
        "mov $0x23, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "timer_idle_proc:\n"
        "popal\n"
        "iret\n"
        : :
        "i"(EMBRYO),
        "i"(HIGHER_HALF_BASE - PGSIZE - sizeof(struct regs_and_interrupt_frame))
        : "ebp", "esp", "eax", "ebx", "ecx", "edx", "memory", "cc"
    );
}

u32 global_ra;
u32 global_esp;
u32 global_ebp;
struct proc *global_curr_proc;

char ctx_switch_temp_stack[2 * PGSIZE];

u32 timer_interrupt_handler_inner() {
    __asm__ volatile("mov 4(%%ebp), %0" : "=r"(global_ra));

    if (!proc_exists) {
        goto timer_handler_default;
    }

    global_curr_proc = get_current_proc();

    __asm__ volatile (
        "mov %%esp, %0\n"
        "mov %%ebp, %1\n"
        :
        "=m"(global_curr_proc->kernel_sp),
        "=m"(global_curr_proc->kernel_bp)
        : : "memory"
    );
    global_esp = global_curr_proc->kernel_sp;
    global_ebp = global_curr_proc->kernel_bp;

    scheduler();
    global_curr_proc = get_current_proc();

    // Restore cr3
    __asm__ volatile (
        "mov %1, %%ebx\n"
        "mov %0, %%eax\n"
        "mov %%eax, %%cr3\n"
        "mov %%ebx, %%esp\n"
        : :
        "m"(global_curr_proc->cr3),
        "i"(ctx_switch_temp_stack + 2 * PGSIZE)
        : "eax", "ebx", "memory"
    );
    // Switch to temporary kernel stack to avoid corrupting new process's kernel stack
    // TODO: this feels super hacky. is there a better solution?

    // struct proc *new_curr_proc = get_current_proc();

    if (global_curr_proc->status != EMBRYO) {
        __asm__ volatile (
            "mov %0, %%esp\n"
            "mov %1, %%ebp\n"
            : :
            "m"(global_curr_proc->kernel_sp),
            "m"(global_curr_proc->kernel_bp)
            : "esp", "memory"
        );
        goto timer_handler_default;
    }
    __asm__ volatile (
        "mov %0, %%esp\n"
        "mov %1, %%ebp\n"
        : :
        "m"(global_esp),
        "m"(global_ebp)
        : "esp"
    );

    // TODO: there's a bug with ebp (I think) that only happens on -O0. fix it

    struct regs_and_interrupt_frame *f = (struct regs_and_interrupt_frame *) (HIGHER_HALF_BASE - PGSIZE - sizeof(*f));

    // Restore registers in interrupt frame
    f->frame.sp = HIGHER_HALF_BASE - 12;
    f->frame.ip = global_curr_proc->entry;
    f->frame.cs = 0x1B;
    f->frame.ss = 0x23;
    f->frame.flags = global_curr_proc->eflags;

    // Restore general purpose registers
    f->regs.eax = global_curr_proc->registers.eax;
    f->regs.ebx = global_curr_proc->registers.ebx;
    f->regs.ecx = global_curr_proc->registers.ecx;
    f->regs.edx = global_curr_proc->registers.edx;
    f->regs.esi = global_curr_proc->registers.esi;
    f->regs.edi = global_curr_proc->registers.edi;
    f->regs.ebp = global_curr_proc->registers.ebp;


    tss.esp0 = HIGHER_HALF_BASE - PGSIZE;

    __asm__ volatile ("movl %0, 0x4(%%ebp)" : : "r"(global_ra) : "memory");

timer_handler_default:
    global_curr_proc = get_current_proc();
    ticks++;
    pic_send_eoi(0);

    if (!proc_exists) {
        return 0;
    } else {
        return global_curr_proc->status;
    }
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

char kbd_shift_US [128] =
{
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',   
  '\t', /* <-- Tab */
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n',     
    0, /* <-- control key */
  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',  0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0,
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

__attribute__ ((naked))
void keyboard_interrupt_handler() {
    __asm__ volatile (
        "pushal\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "call keyboard_interrupt_handler_inner\n"
        "call get_curr_pid\n"
        "test %%eax, %%eax\n"
        "je kb_idle_proc\n"
        "mov $0x23, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "kb_idle_proc:\n"
        "popal\n"
        "iret\n"
        :::
    );
}

// TODO: change this to work like pipes

struct tty_data tty_data = {
    .mode = TTY_MODE_COOKED,
    // If write_ptr == read_ptr, buf is empty
    .read_ptr = 0,
    .write_ptr = 0,
    .internal_write_ptr = 0,
    .left_shift = false,
    .right_shift = false,
};

void keyboard_interrupt_handler_inner() {
    u8 scancode;
    if (inb(0x64) & 1) {
        scancode = inb(0x60);
        
        if (tty_data.mode == TTY_MODE_SCANCODE) {
            tty_data.input_buffer[tty_data.internal_write_ptr] = scancode;
            tty_data.internal_write_ptr = (tty_data.internal_write_ptr + 1) % INPUT_BUFFER_LEN;
            tty_data.write_ptr = tty_data.internal_write_ptr;
            if ((tty_data.internal_write_ptr + 1) % INPUT_BUFFER_LEN == tty_data.read_ptr % INPUT_BUFFER_LEN) {
                // Buffer is full; overwrite the oldest data
                tty_data.read_ptr = (tty_data.read_ptr + 1) % INPUT_BUFFER_LEN;
            }
        } else {
            if (scancode == 0x2A) {
                tty_data.left_shift = true;
            } else if (scancode == 0x36) {
                tty_data.right_shift = true;
            } else if (scancode == 0xAA) {
                tty_data.left_shift = false;
            } else if (scancode == 0xB6) {
                tty_data.right_shift = false;
            }
            if (scancode < 128 && kbd_US[scancode] != 0) {
                char c;
                if (tty_data.left_shift || tty_data.right_shift) {
                    c = kbd_shift_US[scancode];
                } else {
                    c = kbd_US[scancode];
                };
                bool valid = (c != 0);
                if (!valid) {
                    goto keyboard_handler_end;
                }
                // if (c == '\b' && tty_data.mode == TTY_MODE_COOKED) {
                //     if (tty_data.read_ptr != tty_data.internal_write_ptr) {
                //         kprint_char(c);
                //     }
                //     goto keyboard_handler_end;
                // }
                // if (tty_data.mode == TTY_MODE_COOKED) {
                //     kprint_char(c);
                // }
                if (tty_data.mode == TTY_MODE_COOKED) {
                    if (c == '\b') {
                        if (tty_data.read_ptr != tty_data.internal_write_ptr) {
                            kprint_char(c);
                            tty_data.internal_write_ptr = (tty_data.internal_write_ptr - 1) % INPUT_BUFFER_LEN;
                        }
                        goto keyboard_handler_end;
                    } else {
                        kprint_char(c);
                    }
                }
                tty_data.input_buffer[tty_data.internal_write_ptr] = c;
                tty_data.internal_write_ptr = (tty_data.internal_write_ptr + 1) % INPUT_BUFFER_LEN;
                if (tty_data.mode == TTY_MODE_RAW || c == '\n') {
                    tty_data.write_ptr = tty_data.internal_write_ptr;
                }
            }
        }
    }

keyboard_handler_end:
    pic_send_eoi(1);
}

__attribute__((naked))
void syscall_interrupt_handler(void) {
  __asm__ volatile(
      "cli\n"
      "pushal\n"
      // Push the address of the syscall_registers struct that we just
      // constructed on the stack
      "push %%esp\n"
      "mov $0x10, %%ax\n"
      "mov %%ax, %%ds\n"
      "mov %%ax, %%es\n"
      "mov %%ax, %%fs\n"
      "mov %%ax, %%gs\n"
      "call syscall_interrupt_handler_inner\n"
      "add $4, %%esp\n"
      "call get_curr_pid\n"
      // TODO: don't need this test since idle should never make a syscall
      "test %%eax, %%eax\n"
      "je syscall_idle_proc\n"
      "mov $0x23, %%ax\n"
      "mov %%ax, %%ds\n"
      "mov %%ax, %%es\n"
      "mov %%ax, %%fs\n"
      "mov %%ax, %%gs\n"
      "syscall_idle_proc:\n"
      "popal\n"
      "sti\n"
      "iret"
      :::
    );
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
#define IDT_FLAG_USER_INTERRUPT_GATE 0xEE

void 
idt_load() {
    idt_desc.size = sizeof(idt) - 1;
    idt_desc.offset = (u32) &idt;

    idt_write_entry(
        0x0D,
        (u32) gp_fault_handler,
        0x08,
        IDT_FLAG_INTERRUPT_GATE
    );

    idt_write_entry(
        0x0E,
        (u32) page_fault_handler,
        0x08,
        IDT_FLAG_INTERRUPT_GATE
    );

    idt_write_entry(
        0x20,
        (u32) timer_interrupt_handler,
        0x08,
        IDT_FLAG_INTERRUPT_GATE
    );

    idt_write_entry(
        0x21,
        (u32) keyboard_interrupt_handler,
        0x08,
        IDT_FLAG_INTERRUPT_GATE
    );

    idt_write_entry(
        0x80,
        (u32) syscall_interrupt_handler,
        0x08,
        IDT_FLAG_USER_INTERRUPT_GATE
    );

    __asm__ volatile ("lidt %0" : : "m" (idt_desc));
    pic_remap();
    __asm__ volatile ("sti");
}
