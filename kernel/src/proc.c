#include "proc.h"
#include "kprintf.h"
#include "kstring.h"
#include "util.h"

#define MAX_PROCS 256

struct proc ptable[256] = {0};

uint32_t curr_pid = 0;
bool proc_exists = false;

// Return 0 if no proc available.
struct proc *alloc_proc() {
    proc_exists = true;
    for (uint32_t i = 0; i < MAX_PROCS; i++) {
        if (!ptable[i].present) {
            ptable[i].present = true;
            ptable[i].pid = curr_pid;
            curr_pid++;
            return ptable + i;
        }
    }
    return 0;
}

// Return true on success, false on failure.
bool release_proc(struct proc *proc) {
    if (proc->present) {
        return false;
    } else {
        proc->present = false;
        return true;
    }
}
void restore_regs(struct proc *proc) __attribute__ ((naked));

void restore_regs(struct proc *proc) {
    // proc->registers.eax
}

volatile uint32_t scheduler_proc_index = 0;
void scheduler() {
    scheduler_proc_index++;
    for (uint32_t i = scheduler_proc_index; i != scheduler_proc_index - 1; i = (i + 1) % MAX_PROCS) {
        if (ptable[i].present) {
            // struct proc proc = ptable[i];
            scheduler_proc_index = i;
            kprintf("Scheduling process %d!\n", i);
            return;
            // Restore all registers from proc and call iret
        }
    }
}
