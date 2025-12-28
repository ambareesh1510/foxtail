#ifndef PROC_H
#define PROC_H

#include "fs_defs.h"
#include "util.h"

struct saved_registers {
    // Set manually
    u32 eax, ebx, ecx, edx, esi, edi;
    u32 ebp; 
    // uint16_t ds, es, fs, gs;
    // Set by iret
    u32 esp, eip;
    u16 cs, ss;
    u32 eflags;
};

enum proc_status {
    UNUSED,
    EMBRYO,
    RUNNABLE,
    WAITING_ON_PID,
    WAITING_ON_STDIN,
    WAITING_ON_PIPE,
    KILLED,
};

enum fd_status {
    FD_UNMAPPED,
    FD_STDIN,
    FD_STDOUT,
    FD_STDERR,
    FD_REGULAR_FILE,
    FD_REGULAR_DIRECTORY,
    FD_PIPE,
};

struct fd {
    enum fd_status status;
    u32 mode;
    union {
        struct inode *file;
        u32 pipe_idx;
    } data;
    u32 ptr;
};

#define MAX_FDS 16

struct proc {
    bool present;
    u32 pid;
    u32 cr3;
    struct saved_registers registers;
    char name[FILENAME_MAX_LEN];
    // Physical address of the kernel stack. Unused for now
    u32 kernel_stack;
    enum proc_status status;
    u32 parent_pid;
    u32 parent_idx;
    u32 waiting_on;
    u32 kernel_sp;
    u32 kernel_bp;
    u32 brk;
    // Holds the exit code of the last "wait"ed process when this process is still alive;
    // holds its own exit code once it exits.
    u32 exit_code;
    struct inode *cwd;
    struct fd fds[MAX_FDS];
};

#define MAX_PROCS 256
extern struct proc ptable[MAX_PROCS];
extern volatile u32 scheduler_proc_index;

extern bool proc_exists;
struct proc *alloc_proc();

extern u32 sh_proc_pid;
void scheduler();
struct proc *get_current_proc();
u32 get_proc_idx(struct proc *proc);

#endif /* PROC_H */
