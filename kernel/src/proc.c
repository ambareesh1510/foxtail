#include "proc.h"
#include "interrupt.h"
#include "kprintf.h"
#include "kstring.h"
#include "syscall.h"
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
            ptable[i] = (struct proc) {0};
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

bool proc_alive(enum proc_status status) {
    return (status == EMBRYO) || (status == RUNNABLE) || (status == WAITING_ON_PID) || (status == WAITING_ON_PIPE) || (status == WAITING_ON_STDIN);
}

volatile u32 scheduler_proc_index;
// TODO: figure out how to schedule idle process only when there are no other runnable processes
void scheduler() {
    u32 i = scheduler_proc_index;
    for (;;) {
        i = (i + 1) % MAX_PROCS;
        enum proc_status status = ptable[i].status;
        if (status == EMBRYO || status == RUNNABLE) {
            scheduler_proc_index = i;
            // kprintf("scheduling idx=%d pid=%d name=%s status=%d\n", i, ptable[i].pid, ptable[i].name, ptable[i].status);
            return;
        } else if (status == WAITING_ON_PID) {
            if (ptable[ptable[i].waiting_on].status == KILLED) {
                // kprintf("pid %d killed, wake up %d\n", ptable[i].waiting_on, i);
                ptable[i].status = RUNNABLE;
                scheduler_proc_index = i;
                return;
            }
        } else if (status == WAITING_ON_STDIN) {
            if (input_buffer_nonempty) {
                ptable[i].status = RUNNABLE;
                scheduler_proc_index = i;
                return;
            }
        } else if (status == WAITING_ON_PIPE) {
            u32 pipe_idx = ptable[i].waiting_on;
            if (pipe_data[pipe_idx].write_ptr != pipe_data[pipe_idx].read_ptr) {
                ptable[i].status = RUNNABLE;
                scheduler_proc_index = i;
                return;
            }
        } else if (status == KILLED) {
            enum proc_status parent_status = ptable[ptable[i].parent_idx].status;
            if (!proc_alive(parent_status)) {
                ptable[i].status = UNUSED;
            }
        }
    }
    panic("UNREACHABLE: scheduler exited without choosing process\n");
}

struct proc *get_current_proc() {
    return ptable + scheduler_proc_index;
}

u32 get_proc_idx(struct proc *proc) {
    return ((u32) proc - (u32) ptable) / sizeof(struct proc);
}
