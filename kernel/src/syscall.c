#include "syscall.h"
#include "cleanup.h"
#include "exec.h"
#include "fs.h"
#include "fs_defs.h"
#include "interrupt.h"
#include "kprintf.h"
#include "paging.h"
#include "pgalloc.h"
#include "proc.h"
#include "syscall_defs.h"
#include "util.h"
#include "kstring.h"
#include "vga.h"

char pipe_buffers[NUM_PIPE_BUFS][PIPE_BUF_SIZE] = {0};
struct pipe_data pipe_data[NUM_PIPE_BUFS] = {0};

// TODO: write a page fault handler that kills the process so that we can't access random memory

bool is_valid_user_addr(u32 addr) {
    return addr < HIGHER_HALF_BASE;
}

// Writes edx bytes from the buf in ecx to file descriptor ebx.
// Returns eax = 0 on success.
// eax = -1 if error.
// TODO: return number of bytes written (in case we exceed size limit)
void sys_write(struct syscall_registers *s) {
    if (!is_valid_user_addr(s->ecx)) {
        s->eax = SYS_WRITE_BAD_BUF;
        return;
    }
    struct proc *curr_proc = get_current_proc();
    if (s->ebx >= MAX_FDS) {
        s->eax = SYS_WRITE_BAD_FD;
        return;
    }
    enum fd_status status = curr_proc->fds[s->ebx].status;
    if (status !=  FD_REGULAR_FILE && status !=  FD_PIPE && status != FD_STDOUT && status != FD_STDERR) {
        s->eax = SYS_WRITE_BAD_FD;
        return;
    }
    if (!(curr_proc->fds[s->ebx].mode & SYS_OPEN_FILE_MODE_WRITE)) {
        s->eax = SYS_WRITE_BAD_PERMS;
        return;
    }
    char *buf = (char *) s->ecx;
    if (status == FD_STDOUT || status == FD_STDERR) {
        for (u32 i = 0; i < s->edx; i++) {
            kprint_char(buf[i]);
        }
    } else if (status == FD_PIPE) {
        u32 pipe_idx = curr_proc->fds[s->ebx].data.pipe_idx;
        // if (pipe_data[pipe_idx].num_refs < 2) {
        //     // Read end is closed.
        //     // TODO: proper error code
        //     s->eax = -1;
        //     return;
        // }
        for (u32 i = 0; i < s->edx; i++) {
            if (
                (pipe_data[pipe_idx].internal_write_ptr + 1) % PIPE_BUF_SIZE == (pipe_data[pipe_idx].read_ptr) % PIPE_BUF_SIZE
            ) {
                // TODO: return bytes written
                s->eax = -1;
                break;
            }
            pipe_buffers[pipe_idx][pipe_data[pipe_idx].internal_write_ptr] = buf[i];
            pipe_data[pipe_idx].internal_write_ptr = (pipe_data[pipe_idx].internal_write_ptr) + 1 % PIPE_BUF_SIZE;
            if (buf[i] == '\n') {
                pipe_data[pipe_idx].write_ptr = pipe_data[pipe_idx].internal_write_ptr;
            }
        }
    } else {
        struct inode *file = curr_proc->fds[s->ebx].data.file;
        u32 ptr = curr_proc->fds[s->ebx].ptr;
        s->eax = fs_write_bytes(file, ptr, s->edx, buf);
        curr_proc->fds[s->ebx].ptr += s->eax;
        return;
    }
    return;
}

// Read up to edx bytes to the buf at ecx from file descriptor ebx.
// If the file is stdin, blocks until at least one byte is available.
// TODO: will need to rethink blocking implementation
//   when adding multiple cores.
// Returns eax = -1 on error.
// Returns eax = # bytes read on success.
void sys_read(struct syscall_registers *s) {
    if (!is_valid_user_addr(s->ecx)) {
        s->eax = SYS_READ_BAD_BUF;
        return;
    }
    struct proc *curr_proc = get_current_proc();
    // TODO: factor fd-checking helper out
    if (s->ebx >= MAX_FDS) {
        s->eax = SYS_READ_BAD_FD;
        return;
    }
    enum fd_status status = curr_proc->fds[s->ebx].status;
    if (status !=  FD_REGULAR_FILE && status != FD_PIPE && status != FD_STDIN) {
        s->eax = SYS_READ_BAD_FD;
        return;
    }
    if (!(curr_proc->fds[s->ebx].mode & SYS_OPEN_FILE_MODE_READ)) {
        s->eax = SYS_READ_BAD_PERMS;
        return;
    }
    char *buf = (char *) s->ecx;
    if (status == FD_STDIN) {
        if (!input_buffer_nonempty) {
            curr_proc->status = WAITING_ON_STDIN;
            __asm__ volatile ("int $0x20" : : : "memory");
        }
        u32 target = s->edx;
        u32 count = 0;
        for (; count < target; count++) {
            if (!input_buffer_nonempty) {
                break;
            }
            buf[count] = input_buffer[input_buffer_read_ptr];
            input_buffer_read_ptr = (input_buffer_read_ptr + 1) % INPUT_BUFFER_LEN;
            if (input_buffer_read_ptr == input_buffer_write_ptr) {
                input_buffer_nonempty = false;
            }
        }
        s->eax = count;
        return;
    } else if (status == FD_PIPE) {
        u32 pipe_idx = curr_proc->fds[s->ebx].data.pipe_idx;
        if (pipe_data[pipe_idx].read_ptr == pipe_data[pipe_idx].write_ptr) {
            if (pipe_data[pipe_idx].num_refs < 2) {
                // Write end is closed.
                // TODO: proper error code
                s->eax = -1;
                return;
            }
            curr_proc->status = WAITING_ON_PIPE;
            curr_proc->waiting_on = pipe_idx;
            __asm__ volatile ("int $0x20" : : : "memory");
        }
        u32 target = s->edx;
        u32 count = 0;
        for (; count < target; count++) {
            if (pipe_data[pipe_idx].read_ptr == pipe_data[pipe_idx].write_ptr) {
                break;
            }
            buf[count] = pipe_buffers[pipe_idx][pipe_data[pipe_idx].read_ptr];
            pipe_data[pipe_idx].read_ptr = (pipe_data[pipe_idx].read_ptr + 1) % PIPE_BUF_SIZE;
            // pipe_data[pipe_idx].read_ptr = (pipe_data[pipe_idx].read_ptr + 1);
        }
        s->eax = count;
        return;
    } else {
        struct inode *file = curr_proc->fds[s->ebx].data.file;
        u32 ptr = curr_proc->fds[s->ebx].ptr;
        s->eax = fs_read_bytes(file, ptr, s->edx, buf);
        curr_proc->fds[s->ebx].ptr += s->eax;
        return;
    }
}

// Spawns a new process from the file path specified by ebx, with argc = ecx and argv = edx.
// Returns eax = -1 on failure.
// Returns eax = new pid on success.
void sys_spawn_proc(struct syscall_registers *s) {
    if (!is_valid_user_addr(s->ebx) || !is_valid_user_addr(s->edx)) {
        s->eax = -1;
        return;
    }
    struct inode *prog = get_inode_by_path(get_current_proc()->cwd, (char *) s->ebx);
    if (prog == 0) {
        s->eax = -1;
        return;
    }
    prog = follow_symlink(prog);
    if (prog->type != FT_FILE) {
        s->eax = -1;
        return;
    }
    struct proc *new_proc = exec_helper(prog, s->ecx, (char **) s->edx, s->esi, (struct spawn_custom_command *) s->edi);
    if (new_proc == 0) {
        s->eax = -1;
    } else {
        struct proc *curr_proc = get_current_proc();
        new_proc->parent_pid = curr_proc->pid;
        new_proc->parent_idx = get_proc_idx(curr_proc);
        new_proc->cwd = curr_proc->cwd;
        // kprintf("Spawn %d\n", new_proc->pid);
        s->eax = new_proc->pid;
    }
}

void sys_getpid(struct syscall_registers *s) {
    s->eax = get_current_proc()->pid;
}

void sys_exit(
    __attribute__ ((unused)) struct syscall_registers *s
) {
    // TODO: this might not work: sys_exit has a stack frame on the kernel stack, but that kernel stack gets cleaned up in cleanup_proc(). 
    // Instead, we should store kernel stack addr in the proc struct and free it (in scheduler()) once the process is killed.
    // TODO: in scheduler, we need to deal with killed processes that aren't being waited upon.
    struct proc *curr_proc = get_current_proc();
    cleanup_proc(curr_proc);
    curr_proc->exit_code = s->ebx;
    // kprintf("Kill %d\n", get_current_proc()->pid);
    __asm__ volatile ("int $0x20");
}

// If pid doesn't exist, fail
// If pid isn't child of current process, fail
// Otherwise, set state to waiting, set waiting_proc to pid
// Return eax = exit code on success, eax = -1 on failure.
void sys_wait(struct syscall_registers *s) {
    struct proc *curr_proc = get_current_proc();
    u32 pid = s->ebx;
    bool found = false;
    u32 i;
    for (i = 0; i < MAX_PROCS; i++) {
        if (ptable[i].pid == pid) {
            if (ptable[i].parent_pid == get_current_proc()->pid) {
                found = true;
            }
            break;
        }
    }
    if (found) {
        curr_proc->status = WAITING_ON_PID;
        curr_proc->waiting_on = i;
        __asm__ volatile ("int $0x20");
        s->eax = curr_proc->exit_code;
    } else {
        s->eax = -1;
    }
}

u32 sbrk_temp_pgdir[PGDIR_LEN];
void sbrk_helper(struct proc *curr_proc, i32 delta) {
    i32 brk_delta = ((delta + PGSIZE - 1) / PGSIZE) * PGSIZE;
    if (brk_delta == 0) {
        return;
    } 

    // Start allocating at 0x80000000.
    // TODO: when adding resizeable kernel stack, make sure allocation doesn't overlap stack.
    kernel_pgtbl[PGDIR_LEN - 1] = curr_proc->cr3 | 0x3;
    flush_tlb();
    memcpy((char *) sbrk_temp_pgdir, (char *) temp_page_ptr, PGSIZE);
    
    if (brk_delta < 0) {
        brk_delta = max(brk_delta, 0x80000000 - curr_proc->brk);
        for (u32 addr = curr_proc->brk - PGSIZE; addr >= curr_proc->brk + brk_delta; addr -= PGSIZE) {
            // Temp-map the pgtbl, dealloc the page
            u32 pgtbl_phys_addr = sbrk_temp_pgdir[(addr >> 22) & 0x03FF];
            kernel_pgtbl[PGDIR_LEN - 1] = pgtbl_phys_addr;
            flush_tlb();
            u32 page_phys_addr = temp_page_ptr[(addr >> 12) & 0x03FF];
            free_page(page_phys_addr / PGSIZE);
            temp_page_ptr[(addr >> 12) & 0x03FF] = 0;
            // If addr is aligned to a pgdir entry boundary, dealloc the pgtbl
            if (addr == ((addr >> 22) << 22)) {
                free_page(pgtbl_phys_addr / PGSIZE);
                sbrk_temp_pgdir[(addr >> 22) & 0x03FF] = 0;
            }
        }
    } else if (brk_delta > 0) {
        for (u32 addr = curr_proc->brk; addr < curr_proc->brk + brk_delta; addr += PGSIZE) {
            u32 pgtbl_phys_addr = sbrk_temp_pgdir[(addr >> 22) & 0x03FF];
            bool new = false;
            // If pgdir doesn't exist, allocate one
            // TODO: you can do this by checking if aligned to a page boundary
            if (!(pgtbl_phys_addr & 0x1)) {
                pgtbl_phys_addr = alloc_page() * PGSIZE;
                sbrk_temp_pgdir[(addr >> 22) & 0x03FF] = pgtbl_phys_addr | 0x7;
                new = true;
            }
            kernel_pgtbl[PGDIR_LEN - 1] = pgtbl_phys_addr | 0x3;
            flush_tlb();
            if (new) {
                memset((char *) temp_page_ptr, 0, PGSIZE);
            }
            temp_page_ptr[(addr >> 12) & 0x03FF] = alloc_page() * PGSIZE | 0x7;
        }
    }

    kernel_pgtbl[PGDIR_LEN - 1] = curr_proc->cr3 | 0x3;
    flush_tlb();
    memcpy((char *) temp_page_ptr, (char *) sbrk_temp_pgdir, PGSIZE);

    curr_proc->brk += brk_delta;
}

void sys_sbrk(struct syscall_registers *s) {
    struct proc *curr_proc = get_current_proc();

    // Round up brk_delta.
    i32 brk_delta = s->ebx;

    s->eax = curr_proc->brk;
    sbrk_helper(curr_proc, brk_delta);
    return;
}

// Changes directory to the dir specified in ebx.
// Returns eax = -1 on failure, eax = 0 on success.
void sys_cd(struct syscall_registers *s) {
    if (!is_valid_user_addr(s->ebx)) {
        s->eax = -1;
        return;
    }
    struct proc *curr_proc = get_current_proc();
    struct inode *new_cwd = get_inode_by_path(curr_proc->cwd, (char *) s->ebx);
    if (new_cwd == 0) {
        s->eax = -1;
        return;
    }
    curr_proc->cwd = new_cwd;
    s->eax = 0;
}

i32 path_helper(struct inode *inode, char *buf, u32 buf_len) {
    u32 len = 0;
    struct inode *curr = inode;
    while (strcmp(curr->name, FS_ROOT_PATH) != 0) {
        // strlen + path separator ('/')
        len += strlen(curr->name) + 1;
        curr = get_inode_at_idx(curr->parent);
    }
    // strlen + null terminator
    len += strlen(curr->name) + 1;
    if (len > buf_len) {
        return -1;
    }
    // Then copy from the back
    curr = inode;
    len -= 1;
    buf[len] = '\0';
    while (strcmp(curr->name, FS_ROOT_PATH) != 0) {
        u32 name_len = strlen(curr->name);
        len -= name_len;
        memcpy(buf + len, curr->name, name_len);
        len -= 1;
        buf[len] = '/';
        curr = get_inode_at_idx(curr->parent);
    }
    memcpy(buf, curr->name, strlen(curr->name));
    return 0;
}

// Writes the full path of the pwd into the buf at ebx (whose length is ecx).
// Fails if the buf isn't large enough to accomodate the full path + null terminator.
void sys_pwd(struct syscall_registers *s) {
    // Recurse up the tree and compute the length of the path
    if (!is_valid_user_addr(s->ebx)) {
        s->eax = -1;
        return;
    }
    struct proc *curr_proc = get_current_proc();
    s->eax = path_helper(curr_proc->cwd, (char *) s->ebx, s->ecx);
    return;
}

void sys_open(struct syscall_registers *s) {
    if (!is_valid_user_addr(s->ebx)) {
        s->eax = -1;
        return;
    }
    char *path = (char *) s->ebx;
    struct proc *curr_proc = get_current_proc();
    struct inode *target = get_inode_by_path(curr_proc->cwd, path);
    if (target == 0) {
        s->eax = -1;
        return;
    }
    // TODO: add permissions checking
    for (u32 i = 0; i < MAX_FDS; i++) {
        if (curr_proc->fds[i].status == FD_UNMAPPED) {
            if (target->type == FT_FILE) {
                curr_proc->fds[i].status = FD_REGULAR_FILE;
            } else if (target->type == FT_DIRECTORY) {
                curr_proc->fds[i].status = FD_REGULAR_DIRECTORY;
            } else if (target->type == FT_SYMLINK) {
                struct inode *symlink_target = follow_symlink(target);
                if (symlink_target->type == FT_FILE) {
                    curr_proc->fds[i].status = FD_REGULAR_FILE;
                } else if (symlink_target->type == FT_DIRECTORY) {
                    curr_proc->fds[i].status = FD_REGULAR_DIRECTORY;
                } else {
                    panic("Unreachable\n");
                }
            } else {
                panic("Unreachable\n");
            }
            curr_proc->fds[i].data.file = target;
            acquire_inode(target);
            curr_proc->fds[i].mode = s->ecx;
            curr_proc->fds[i].ptr = 0;
            s->eax = i;
            return;
        }
    }
    s->eax = -1;
    return;
}

// TODO: test
void sys_reopen(struct syscall_registers *s) {
    if (s->ebx > MAX_FDS) {
        s->eax = -1;
        return;
    }
    if (!is_valid_user_addr(s->ecx)) {
        s->eax = -1;
        return;
    }
    char *path = (char *) s->ecx;
    struct proc *curr_proc = get_current_proc();
    struct inode *target = get_inode_by_path(curr_proc->cwd, path);
    if (target == 0) {
        s->eax = -1;
        return;
    }

    enum fd_status status = curr_proc->fds[s->ebx].status;
    if (status == FD_REGULAR_FILE || status == FD_REGULAR_DIRECTORY) {
        release_inode(curr_proc->fds[s->ebx].data.file);
    }

    // TODO: add permissions checking
    // if (target->type == FT_FILE) {
    //     curr_proc->fds[s->ebx].status = FD_REGULAR_FILE;
    // } else {
    //     curr_proc->fds[s->ebx].status = FD_REGULAR_DIRECTORY;
    // }
    if (target->type == FT_FILE) {
        curr_proc->fds[s->ebx].status = FD_REGULAR_FILE;
    } else if (target->type == FT_DIRECTORY) {
        curr_proc->fds[s->ebx].status = FD_REGULAR_DIRECTORY;
    } else if (target->type == FT_SYMLINK) {
        struct inode *symlink_target = get_inode_at_idx(target->data.symlink_data.target);
        u32 depth = 0;
        while (symlink_target->type == FT_SYMLINK) {
            depth++;
            symlink_target = get_inode_at_idx(symlink_target->data.symlink_data.target);
            if (symlink_target->type == FT_UNALLOCATED) {
                s->eax = -1;
                return;
            }
            if (depth > SYMLINK_RECURSION_LIMIT) {
                s->eax = -1;
                return;
            }
        }
        if (symlink_target->type == FT_FILE) {
            curr_proc->fds[s->ebx].status = FD_REGULAR_FILE;
        } else if (symlink_target->type == FT_DIRECTORY) {
            curr_proc->fds[s->ebx].status = FD_REGULAR_DIRECTORY;
        } else {
            panic("Unreachable\n");
        }
    } else {
        panic("Unreachable\n");
    }
    curr_proc->fds[s->ebx].status = FD_REGULAR_FILE;
    curr_proc->fds[s->ebx].data.file = target;
    acquire_inode(target);
    curr_proc->fds[s->ebx].mode = s->edx;
    curr_proc->fds[s->ebx].ptr = 0;
    s->eax = s->ebx;
    return;
}

void sys_close(struct syscall_registers *s) {
    if (s->ebx >= MAX_FDS) {
        s->eax = -1;
        return;
    }
    struct proc *curr_proc = get_current_proc();
    if (curr_proc->fds[s->ebx].status == FD_UNMAPPED) {
        s->eax = -1;
        return;
    }
    enum fd_status status = curr_proc->fds[s->ebx].status;
    if (status == FD_REGULAR_FILE || status == FD_REGULAR_DIRECTORY) {
        release_inode(curr_proc->fds[s->ebx].data.file);
    } else if (status == FD_PIPE) {
        pipe_data[curr_proc->fds[s->ebx].data.pipe_idx].num_refs--;
    }
    curr_proc->fds[s->ebx].status = FD_UNMAPPED;
    s->eax = 0;
    return;
}

void sys_set_ptr(struct syscall_registers *s) {
    if (s->ebx >= MAX_FDS) {
        s->eax = -1;
        return;
    }
    struct proc *curr_proc = get_current_proc();
    if (curr_proc->fds[s->ebx].status != FD_REGULAR_FILE) {
        s->eax = -1;
        return;
    }
    u32 ptr = min(
        s->ecx,
        curr_proc->fds[s->ebx].data.file->data.file_data.size
    );
    curr_proc->fds[s->ebx].ptr = ptr;
    s->eax = ptr;
    return;
}

// File type of fd in ebx.
void sys_ftype(struct syscall_registers *s) {
    if (s->ebx >= MAX_FDS) {
        s->eax = SYS_FTYPE_BAD_FD;
        return;
    }
    struct proc *curr_proc = get_current_proc();
    enum filetype ft = curr_proc->fds[s->ebx].data.file->type;
    if (ft == FT_FILE) {
        s->eax = SYS_FTYPE_FILE;
        return;
    } else if (ft == FT_DIRECTORY) {
        s->eax = SYS_FTYPE_DIR;
        return;
    } else if (ft == FT_SYMLINK) {
        s->eax = SYS_FTYPE_SYMLINK;
        return;
    } else {
        s->eax = SYS_FTYPE_BAD_FD;
        return;
    }
}

// Writes file info of fd ebx to (struct file_info *) in ecx.
void sys_file_info(struct syscall_registers *s) {
    if (s->ebx >= MAX_FDS) {
        s->eax = -1;
        return;
    }
    struct proc *curr_proc = get_current_proc();
    if (curr_proc->fds[s->ebx].data.file->type != FT_FILE) {
        s->eax = -1;
        return;
    }
    if (!is_valid_user_addr(s->ecx)) {
        s->eax = -1;
        return;
    }
    struct file_info *info = (struct file_info *) s->ecx;
    info->size = curr_proc->fds[s->ebx].data.file->data.file_data.size;
    s->eax = 0;
}

// Writes dir info of fd ebx to (struct dir_info *) in ecx.
void sys_dir_info(struct syscall_registers *s) {
    if (s->ebx >= MAX_FDS) {
        s->eax = -1;
        return;
    }
    struct proc *curr_proc = get_current_proc();
    if (curr_proc->fds[s->ebx].data.file->type != FT_DIRECTORY) {
        s->eax = -1;
        return;
    }
    if (!is_valid_user_addr(s->ecx)) {
        s->eax = -1;
        return;
    }
    struct dir_info *info = (struct dir_info *) s->ecx;
    info->num_entries = curr_proc->fds[s->ebx].data.file->data.directory_data.num_entries;
    s->eax = 0;
}

// Writes dirent info of entry inside fd ebx, with offset provided in the (struct dirent_info *) ecx, to ecx.
void sys_dirent_info(struct syscall_registers *s) {
    if (s->ebx >= MAX_FDS) {
        s->eax = -1;
        return;
    }
    struct proc *curr_proc = get_current_proc();
    if (curr_proc->fds[s->ebx].data.file->type != FT_DIRECTORY) {
        s->eax = -1;
        return;
    }
    if (!is_valid_user_addr(s->ecx)) {
        s->eax = -1;
        return;
    }
    struct dirent_info *info = (struct dirent_info *) s->ecx;
    struct inode *dir = curr_proc->fds[s->ebx].data.file;
    // TODO: update when adding indirect blocks
    if (info->offset > dir->data.directory_data.num_entries) {
        s->eax = -1;
        return;
    }
    struct inode *dirent_inode = get_inode_at_idx(
        dir->data.directory_data.direct_files[info->offset]
    );
    strcpy(
        info->name,
        dirent_inode->name
    );
    info->offset++;
    s->eax = 0;
    return;
}

// Writes the path of the symlink target ebx into the buf at ecx (which has length edx).
void sys_symlink_info(struct syscall_registers *s) {
    if (s->ebx >= MAX_FDS) {
        s->eax = -1;
        return;
    }
    struct proc *curr_proc = get_current_proc();
    if (curr_proc->fds[s->ebx].data.file->type != FT_SYMLINK) {
        s->eax = -1;
        return;
    }
    if (!is_valid_user_addr(s->ecx)) {
        s->eax = -1;
        return;
    }
    struct inode *target = follow_symlink(curr_proc->fds[s->ebx].data.file);
    s->eax = path_helper(target, (char *) s->ecx, s->edx);
    return;
}

// Create an inode named ecx at the path in ebx.
// If edx = 0, then it is a file; if edx = 1, then it is a directory.
void sys_create(struct syscall_registers *s) {
    if (!is_valid_user_addr(s->ebx) || !is_valid_user_addr(s->ecx)) {
        s->eax = -1;
        return;
    }
    if (s->edx != SYS_CREATE_FILE && s->edx != SYS_CREATE_DIR) {
        s->eax = -1;
        return;
    }
    struct proc *curr_proc = get_current_proc();
    char *parent_path = (char *) s->ebx;
    struct inode *parent_inode = get_inode_by_path(curr_proc->cwd, parent_path);
    if (parent_inode == 0) {
        s->eax = -1;
        return;
    }
    if (parent_inode->type != FT_DIRECTORY) {
        s->eax = -1;
        return;
    }
    if (parent_inode->data.directory_data.num_entries >= DIR_MAX_ENTRIES) {
        s->eax = -1;
        return;
    }
    char *new_path = (char *) s->ecx;
    u32 new_path_len = strlen(new_path);
    if (new_path_len > FILENAME_MAX_LEN - 1) {
        s->eax = -1;
        return;
    }
    for (u32 i = 0; i < new_path_len; i++) {
        // TODO: check contains path separator
        if (new_path[i] == '/') {
            s->eax = -1;
            return;
        }
    }
    if (get_inode_by_path(parent_inode, new_path) != 0) {
        s->eax = -1;
        return;
    }
    struct inode *new_inode = alloc_inode();
    if (new_inode == 0) {
        s->eax = -1;
        return;
    }
    strcpy(new_inode->name, new_path);
    new_inode->parent = get_index_from_inode(parent_inode);
    new_inode->valid = 1;
    if (s->edx == SYS_CREATE_FILE) {
        new_inode->type = FT_FILE;
        new_inode->data.file_data.size = 0;
    } else {
        new_inode->type = FT_DIRECTORY;
        new_inode->data.directory_data.num_entries = 0;
    }
    u32 curr_num_entries = parent_inode->data.directory_data.num_entries;
    parent_inode->data.directory_data.direct_files[curr_num_entries] = get_index_from_inode(new_inode);
    parent_inode->data.directory_data.num_entries++;
    s->eax = 0;
    return;
}

// Delete the file at the path in ebx.
void sys_delete(struct syscall_registers *s) {
    if (!is_valid_user_addr(s->ebx)) {
        s->eax = -1;
        return;
    }
    char *path = (char *) s->ebx;
    struct proc *curr_proc = get_current_proc();
    struct inode *inode = get_inode_by_path(curr_proc->cwd, path);
    if (inode == 0) {
        s->eax = -1;
        return;
    }
    if (inode->type == FT_DIRECTORY && inode->data.directory_data.num_entries != 0) {
        s->eax = -1;
        return;
    }
    u32 inode_idx = get_index_from_inode(inode);
    struct inode *parent = get_inode_at_idx(inode->parent);
    for (u32 i = 0; i < parent->data.directory_data.num_entries; i++) {
        if (parent->data.directory_data.direct_files[i] == inode_idx) {
            parent->data.directory_data.direct_files[i] = 0;
            for (u32 j = i; j < parent->data.directory_data.num_entries - 1; j++) {
                parent->data.directory_data.direct_files[j] = parent->data.directory_data.direct_files[j + 1];
            }
            parent->data.directory_data.num_entries--;
            i--;
        }
    }
    if (inode->num_refs == 0) {
        free_inode(inode);
    } else {
        inode->valid = 0;
    }
    s->eax = 0;
    return;
}

// Create a symlink in dir ecx with filename edx, pointing to ebx.
void sys_link(struct syscall_registers *s) {
    if (!is_valid_user_addr(s->ebx) || !is_valid_user_addr(s->ecx) || !is_valid_user_addr(s->edx)) {
        s->eax = -1;
        return;
    }

    struct proc *curr_proc = get_current_proc();
    struct inode *target = get_inode_by_path(curr_proc->cwd, (char *) s->ebx);
    if (target == 0) {
        s->eax = -1;
        return;
    }
    struct inode *link_dir = get_inode_by_path(curr_proc->cwd, (char *) s->ecx);
    if (link_dir == 0) {
        s->eax = -1;
        return;
    }
    if (link_dir->type != FT_DIRECTORY) {
        s->eax = -1;
        return;
    }
    if (link_dir->data.directory_data.num_entries >= DIR_MAX_ENTRIES) {
        s->eax = -1;
        return;
    }
    char *new_path = (char *) s->edx;
    u32 new_path_len = strlen(new_path);
    if (new_path_len > FILENAME_MAX_LEN - 1) {
        s->eax = -1;
        return;
    }
    for (u32 i = 0; i < new_path_len; i++) {
        // TODO: check contains path separator
        if (new_path[i] == '/') {
            s->eax = -1;
            return;
        }
    }
    struct inode *link_inode = get_inode_by_path(link_dir, new_path);
    if (link_inode != 0 && link_inode->type != FT_SYMLINK) {
        s->eax = -1;
        return;
    }
    if (link_inode == 0) {
        link_inode = alloc_inode();
        if (link_inode == 0) {
            s->eax = -1;
            return;
        }
    }
    strcpy(link_inode->name, new_path);
    link_inode->parent = get_index_from_inode(link_dir);
    link_inode->valid = 1;
    link_inode->type = FT_SYMLINK;
    link_inode->data.symlink_data.target = get_index_from_inode(target);

    u32 curr_num_entries = link_dir->data.directory_data.num_entries;
    link_dir->data.directory_data.direct_files[curr_num_entries] = get_index_from_inode(link_inode);
    link_dir->data.directory_data.num_entries++;
    s->eax = 0;
    return;
}

// Harden the symlink at ebx.
void sys_harden(struct syscall_registers *s) {
    if (!is_valid_user_addr(s->ebx)) {
        s->eax = -1;
        return;
    }
    char *path = (char *) s->ebx;
    struct proc *curr_proc = get_current_proc();
    struct inode *symlink = get_inode_by_path(curr_proc->cwd, path);
    if (symlink == 0) {
        s->eax = -1;
        return;
    }
    if (symlink->type != FT_SYMLINK) {
        s->eax = -1;
        return;
    }
    struct inode *symlink_target = get_inode_at_idx(symlink->data.symlink_data.target);
    if (symlink_target->type != FT_FILE && symlink_target->type != FT_DIRECTORY) {
        s->eax = -1;
        return;
    }

    // Copy all data from file to symlink except for names.
    // Update symlink in old file.
    char temp_name[FILENAME_MAX_LEN];
    memcpy(temp_name, symlink->name, FILENAME_MAX_LEN);
    memcpy((char *) symlink, (char *) symlink_target, sizeof(struct inode));
    memcpy(symlink->name, temp_name, FILENAME_MAX_LEN);
    symlink_target->type = FT_SYMLINK;
    symlink_target->data.symlink_data.target = get_index_from_inode(symlink);
    s->eax = 0;
    return;
}


// Create a pipe, open two fds, and write them to the (struct pipe *) in ebx.
void sys_pipe(struct syscall_registers *s) {
    if (!is_valid_user_addr(s->ebx)) {
        s->eax = -1;
        return;
    }
    struct proc *curr_proc = get_current_proc();
    i32 read_fd = -1, write_fd = -1;
    for (u32 i = 0; i < MAX_FDS; i++) {
        if (curr_proc->fds[i].status == FD_UNMAPPED) {
            if (read_fd == -1) {
                read_fd = i;
            } else {
                write_fd = i;
                break;
            }
        }
    }
    if (read_fd == -1 || write_fd == -1) {
        s->eax = -1;
        return;
    }
    i32 pipe_idx = -1;
    for (u32 i = 0; i < NUM_PIPE_BUFS; i++) {
        if (pipe_data[i].num_refs == 0) {
            pipe_idx = i;
            break;
        }
    }
    if (pipe_idx == -1) {
        s->eax = -1;
        return;
    }

    pipe_data[pipe_idx].read_ptr = 0;
    pipe_data[pipe_idx].write_ptr = 0;
    pipe_data[pipe_idx].internal_write_ptr = 0;
    pipe_data[pipe_idx].num_refs = 2;

    curr_proc->fds[read_fd].status = FD_PIPE;
    curr_proc->fds[read_fd].mode = SYS_OPEN_FILE_MODE_READ;
    curr_proc->fds[read_fd].data.pipe_idx = pipe_idx;
    curr_proc->fds[read_fd].ptr = 0;

    curr_proc->fds[write_fd].status = FD_PIPE;
    curr_proc->fds[write_fd].mode = SYS_OPEN_FILE_MODE_WRITE;
    curr_proc->fds[write_fd].data.pipe_idx = pipe_idx;
    curr_proc->fds[write_fd].ptr = 0;

    struct pipe *pipe = (struct pipe *) s->ebx;
    pipe->read_fd = read_fd;
    pipe->write_fd = write_fd;
    
    s->eax = 0;
    return;
}

void syscall_interrupt_handler_inner(struct syscall_registers *s) {
    // kprintf("Syscall with eax = %x, ebx = %x, ecx = %x, edx = %x\n", s->eax, s->ebx, s->ecx, s->edx);
    switch (s->eax) {
        case SYS_WRITE:
            sys_write(s);
            break;
        case SYS_READ:
            sys_read(s);
            break;
        case SYS_SPAWN_PROC:
            sys_spawn_proc(s);
            break;
        case SYS_GETPID:
            sys_getpid(s);
            break;
        case SYS_EXIT:
            sys_exit(s);
            break;
        case SYS_WAIT:
            sys_wait(s);
            break;
        case SYS_SBRK:
            sys_sbrk(s);
            break;
        case SYS_CD:
            sys_cd(s);
            break;
        case SYS_PWD:
            sys_pwd(s);
            break;
        case SYS_OPEN:
            sys_open(s);
            break;
        case SYS_REOPEN:
            sys_reopen(s);
            break;
        case SYS_CLOSE:
            sys_close(s);
            break;
        case SYS_SET_PTR:
            sys_set_ptr(s);
            break;
        case SYS_FTYPE:
            sys_ftype(s);
            break;
        case SYS_FILE_INFO:
            sys_file_info(s);
            break;
        case SYS_DIR_INFO:
            sys_dir_info(s);
            break;
        case SYS_DIRENT_INFO:
            sys_dirent_info(s);
            break;
        case SYS_SYMLINK_INFO:
            sys_symlink_info(s);
            break;
        case SYS_CREATE:
            sys_create(s);
            break;
        case SYS_DELETE:
            sys_delete(s);
            break;
        case SYS_LINK:
            sys_link(s);
            break;
        case SYS_HARDEN:
            sys_harden(s);
            break;
        case SYS_PIPE:
            sys_pipe(s);
            break;
        default:
            kprintf("Invalid syscall code: %d\n", s->eax);
            break;
    }
}

