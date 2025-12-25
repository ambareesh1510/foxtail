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
    SYS_OPEN,
    SYS_REOPEN,
    SYS_CLOSE,
    SYS_SET_PTR,
};

#define SYS_WRITE_SUCCESS 0
#define SYS_WRITE_BAD_BUF -1
#define SYS_WRITE_BAD_PERMS -2

// Write the null-terminated string `str` to stdout.
// Returns: 0 on success, -1 on error.
static inline int write(int fd, const char *buf, unsigned int count) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WRITE), "b"(fd), "c"(buf), "d"(count)
        : "memory"
    );
    return ret;
}

#define SYS_READ_BAD_BUF -1
#define SYS_READ_BAD_PERMS -2

// Read up to `count` bytes from `fd` into `buf`.
// Returns: number of bytes read, or -1 on error
static inline int read(int fd, char *buf, unsigned int count) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_READ), "b"(fd), "c"(buf), "d"(count)
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

#define SYS_OPEN_FILE_MODE_READ  (1 << 1)
#define SYS_OPEN_FILE_MODE_WRITE (1 << 2)

static inline int open(const char *path, unsigned int mode) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_OPEN), "b"(path), "c"(mode)
        : "memory"
    );
    return ret; 
}

static inline int reopen(unsigned int fd, const char *path, unsigned int mode) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_REOPEN), "b"(fd), "c"(path), "d"(mode)
        : "memory"
    );
    return ret; 
}

static inline int close(unsigned int fd) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_CLOSE), "b"(fd)
        : "memory"
    );
    return ret; 
}

static inline int set_ptr(unsigned int fd, unsigned int ptr) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SET_PTR), "b"(fd), "c"(ptr)
        : "memory"
    );
    return ret; 
}

#endif /* SYSCALL_DEFS_H */
