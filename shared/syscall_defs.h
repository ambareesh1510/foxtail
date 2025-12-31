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
    SYS_FTYPE,
    SYS_FILE_INFO,
    SYS_DIR_INFO,
    SYS_DIRENT_INFO,
    SYS_SYMLINK_INFO,
    SYS_CREATE,
    SYS_DELETE,
    SYS_LINK,
    SYS_HARDEN,
    SYS_PIPE,
};

#define SYS_WRITE_SUCCESS 0
#define SYS_WRITE_BAD_BUF -1
#define SYS_WRITE_BAD_FD -2
#define SYS_WRITE_BAD_PERMS -3

// Write the null-terminated string `str` to stdout.
// Returns: 0 on success, negative return code on error.
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
#define SYS_READ_BAD_FD -2
#define SYS_READ_BAD_PERMS -3

// Read up to `count` bytes from `fd` into `buf`.
// Returns: 0 on success, negative return code on error.
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

enum spawn_custom_command_type {
    SPAWN_CUSTOM_COMMAND_REMAP_FDS,
};

struct spawn_custom_command {
    enum spawn_custom_command_type type;
    union {
        struct {
            unsigned int curr_fd;
            unsigned int new_fd;
        } remap_fds;
    } data;
};

// Spawn a new process from the given `path`.
// Returns: PID of new process on success, -1 on error.
static inline int spawn_proc(const char *path, unsigned int argc, char **argv, unsigned int num_custom_commands, struct spawn_custom_command *commands) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SPAWN_PROC), "b"(path), "c"(argc), "d"(argv), "S"(num_custom_commands), "D"(commands)
        : "memory"
    );
    return ret;
}

// Get PID of current process.
// Returns: PID of current process.
static inline int getpid(void) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETPID)
    );
    return ret;
}

// Exit current process with the given `exit_code`.
// Does not return.
__attribute__((noreturn))
static inline void exit(int exit_code) {
    __asm__ volatile(
        "int $0x80"
        : 
        : "a"(SYS_EXIT), "b"(exit_code)
    );
    // Should never reach here
    while (1);
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

// Change directories to the given `path`.
// Returns: 0 on success, -1 on failure.
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

// Write the current working directory to the given `buf` (whose length is `size`).
// Returns: 0 on success, -1 on failure.
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

// Open the file at `path` in the given `mode`.
// Returns: new file descriptor on success, -1 on failure.
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

// Replace the given `fd` by opening the file at `path` in the given `mode`.
// Returns: 0 on success, -1 on failure.
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

// Close the given `fd`.
// Returns: 0 on success, -1 on failure.
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

// Set the pointer of `fd` to `ptr`.
// Returns: 0 on success, -1 on failure.
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

#define SYS_FTYPE_FILE 1
#define SYS_FTYPE_DIR 2
#define SYS_FTYPE_SYMLINK 3
#define SYS_FTYPE_BAD_FD -1

static inline int ftype(unsigned int fd) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_FTYPE), "b"(fd)
        : "memory"
    );
    return ret; 
}

struct file_info {
    unsigned int size;
};

static inline int file_info(unsigned int fd, struct file_info *info) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_FILE_INFO), "b"(fd), "c"(info)
        : "memory"
    );
    return ret; 
}

struct dir_info {
    unsigned int num_entries;
};

static inline int dir_info(unsigned int fd, struct dir_info *info) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_DIR_INFO), "b"(fd), "c"(info)
        : "memory"
    );
    return ret; 
}

struct dirent_info {
    unsigned int offset;
    char name[28];
};

static inline int dirent_info(unsigned int fd, struct dirent_info *info) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_DIRENT_INFO), "b"(fd), "c"(info)
        : "memory"
    );
    return ret; 
}

static inline int symlink_info(unsigned int fd, char *buf, unsigned int len) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_SYMLINK_INFO), "b"(fd), "c"(buf), "d"(len)
        : "memory"
    );
    return ret; 
}

#define SYS_CREATE_FILE 0
#define SYS_CREATE_DIR 1

static inline int create(const char *parent_path, const char *new_path, unsigned int type) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_CREATE), "b"(parent_path), "c"(new_path), "d"(type)
        : "memory"
    );
    return ret; 
}

static inline int delete(const char *path) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_DELETE), "b"(path)
        : "memory"
    );
    return ret; 
}

static inline int link(const char *target, const char *link_dir, const char *link_path) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_LINK), "b"(target), "c"(link_dir), "d"(link_path)
        : "memory"
    );
    return ret; 
}

static inline int harden(const char *symlink) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_HARDEN), "b"(symlink)
        : "memory"
    );
    return ret; 
}

struct pipe {
    unsigned int read_fd;
    unsigned int write_fd;
};

static inline int pipe(struct pipe *pipe) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_PIPE), "b"(pipe)
        : "memory"
    );
    return ret; 
}

#endif /* SYSCALL_DEFS_H */
