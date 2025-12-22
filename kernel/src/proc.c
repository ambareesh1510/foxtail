#include "proc.h"
#include "kprintf.h"
#include "kstring.h"
#include "util.h"
#include "vga.h"

struct proc ptable[MAX_PROCS] = {0};

uint32_t curr_pid = 0;
bool proc_exists = false;

// Return 0 if no proc available.
struct proc *alloc_proc() {
    proc_exists = true;
    for (uint32_t i = 0; i < MAX_PROCS; i++) {
        if (!ptable[i].present) {
            ptable[i].pid = curr_pid;
            // kprintf("just allocated pid %d\n", curr_pid);
            curr_pid++;
            return ptable + i;
        }
    }
    panic("can't alloc proc\n");
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

volatile uint32_t scheduler_proc_index = 0;
void scheduler() {
    uint32_t i = scheduler_proc_index;
    for (;;) {
        i = (i + 1) % MAX_PROCS;
        enum proc_status status = ptable[i].status;
        // kprintf("Found idx=%d has status %d\n", i, status);
        if (status == EMBRYO || status == RUNNABLE) {
            scheduler_proc_index = i;
            // kprintf("scheduling idx=%d pid=%d\n", i, ptable[i].pid);
            return;
        } else if (status == WAITING) {
            if (ptable[ptable[i].waiting_on].status == KILLED) {
                ptable[i].status = RUNNABLE;
            }
        }
    }
    panic("UNREACHABLE: scheduler exited without choosing process\n");
}

struct proc *get_current_proc() {
    return ptable + scheduler_proc_index;
}
