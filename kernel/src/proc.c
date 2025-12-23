#include "proc.h"
#include "interrupt.h"
#include "kprintf.h"
#include "kstring.h"
#include "util.h"
#include "vga.h"

struct proc ptable[MAX_PROCS] = {0};

u32 curr_pid = 0;
bool proc_exists = false;

// Return 0 if no proc available.
struct proc *alloc_proc() {
    proc_exists = true;
    for (u32 i = 0; i < MAX_PROCS; i++) {
        if (ptable[i].status == UNUSED) {
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

volatile u32 scheduler_proc_index;
// TODO: figure out how to schedule idle process only when there are no other runnable processes
void scheduler() {
    u32 i = scheduler_proc_index;
    u32 since_last_runnable = 0;
    for (;;) {
        since_last_runnable++;
        i = (i + 1) % MAX_PROCS;
        enum proc_status status = ptable[i].status;
        if (i == 1 || i == 0) {
        // kprintf("Found idx=%d has status %d\n", i, status);
        }
        if (status == EMBRYO || status == RUNNABLE) {
            scheduler_proc_index = i;
            since_last_runnable = 0;
            // kprintf("scheduling idx=%d pid=%d name=%s status=%d\n", i, ptable[i].pid, ptable[i].name, ptable[i].status);
            return;
        } else if (status == WAITING_ON_PID) {
            if (ptable[ptable[i].waiting_on].status == KILLED) {
                ptable[i].status = RUNNABLE;
                since_last_runnable = 0;
            }
        } else if (status == WAITING_ON_READ) {
            if (input_buffer_nonempty) {
                ptable[i].status = RUNNABLE;
                since_last_runnable = 0;
            }
        }
        // if (since_last_runnable == MAX_PROCS) {
        //     // since_last_runnable = 0;
        //     // __asm__ volatile ("sti; hlt; cli");
        // }
    }
    panic("UNREACHABLE: scheduler exited without choosing process\n");
}

struct proc *get_current_proc() {
    return ptable + scheduler_proc_index;
}
