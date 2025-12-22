#ifndef SYSCALL_DEFS_H
#define SYSCALL_DEFS_H

enum syscall_code {
    SYS_WRITE,
    SYS_READ,
    SYS_SPAWN_PROC,
    SYS_GETPID,
    SYS_EXIT,
    SYS_WAIT,
};

#endif /* SYSCALL_DEFS_H */
