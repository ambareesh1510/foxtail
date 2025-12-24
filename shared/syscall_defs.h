#ifndef SYSCALL_DEFS_H
#define SYSCALL_DEFS_H

enum syscall_code {
    SYS_WRITE,
    SYS_READ,
    SYS_SPAWN_PROC,
    SYS_GETPID,
    SYS_EXIT,
    SYS_WAIT,
    SYS_SBRK,
    SYS_CD,
    SYS_PWD,
};

// Write the null-terminated string `str` to stdout.
// Returns: 0 on success, -1 on error.
static inline int write(const char *str) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WRITE), "b"(str)
        : "memory"
    );
    return ret;
}

// Read up to `count` bytes from stdin into `buf`.
// Returns: number of bytes read, or -1 on error
static inline int read(char *buf, unsigned int count) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_READ), "b"(buf), "c"(count)
        : "memory"
    );
    return ret;
}

// Spawn a new process from the given `path`.
// Returns: PID of new process on success, -1 on error
static inline int spawn_proc(const char *path) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SPAWN_PROC), "b"(path)
        : "memory"
    );
    return ret;
}

// Get PID of current process.
// Returns: PID of current process
static inline int getpid(void) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETPID)
    );
    return ret;
}

// Exit current process
// Does not return
static inline void exit(void) {
    __asm__ volatile(
        "int $0x80"
        : 
        : "a"(SYS_EXIT)
    );
    // Should never reach here
    while(1);
}

// Wait for child process with given `pid` to exit.
// Returns: 0 on success, -1 if PID doesn't exist or isn't a child
static inline int wait(int pid) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WAIT), "b"(pid)
        : "memory"
    );
    return ret;
}

// Shift break by `delta` bytes (rounded up to the nearest multiple of page size (4096)).
// Returns: old break.
static inline char *sbrk(int delta) {
    char *ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SBRK), "b"(delta)
        : "memory"
    );
    return ret; 
}

static inline int cd(const char *path) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_CD), "b"(path)
        : "memory"
    );
    return ret; 
}

static inline int pwd(char *buf, unsigned int size) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_PWD), "b"(buf), "c"(size)
        : "memory"
    );
    return ret; 
}

#endif /* SYSCALL_DEFS_H */
