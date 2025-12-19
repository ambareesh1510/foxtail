#ifndef PROC_H
#define PROC_H

#include "fs_defs.h"
#include "util.h"

struct saved_registers {
    // Set manually
    uint32_t eax, ebx, ecx, edx, esi, edi;
    uint32_t ebp; 
    // uint16_t ds, es, fs, gs;
    // Set by iret
    uint32_t esp, eip;
    uint16_t cs, ss;
    uint32_t eflags;
};

struct proc {
    bool present;
    uint32_t pid;
    uint32_t cr3;
    struct saved_registers registers;
    char name[FILENAME_MAX_LEN];
    // Physical address of the kernel stack. Unused for now
    uint32_t kernel_stack;
};

#define MAX_PROCS 256
extern struct proc ptable[256];
extern volatile uint32_t scheduler_proc_index;

extern bool proc_exists;
struct proc *alloc_proc();

// void scheduler() __attribute__ ((no_caller_saved_registers));
void scheduler();

#endif /* PROC_H */
