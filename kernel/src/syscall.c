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
    if (status !=  FD_REGULAR_FILE && status != FD_STDOUT && status != FD_STDERR) {
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
    } else {
        struct inode *file = curr_proc->fds[s->ebx].file;
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
    if (status !=  FD_REGULAR_FILE && status != FD_STDIN) {
        s->eax = SYS_READ_BAD_FD;
        return;
    }
    if (!(curr_proc->fds[s->ebx].mode & SYS_OPEN_FILE_MODE_READ)) {
        s->eax = SYS_READ_BAD_PERMS;
        return;
    }
    char *buf = (char *) s->ecx;
    if (status == FD_STDIN) {
        u32 target = s->edx;
        // kprintf("before &s=%x\n", &s);
        u32 esp;
        __asm__ volatile ("mov %%esp, %0" : "=r"(esp));
        if (!input_buffer_nonempty) {
            struct proc *curr_proc = get_current_proc();
            curr_proc->status = WAITING_ON_READ;
            __asm__ volatile ("int $0x20" : : : "memory");
        }
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
    } else {
        struct inode *file = curr_proc->fds[s->ebx].file;
        u32 ptr = curr_proc->fds[s->ebx].ptr;
        s->eax = fs_read_bytes(file, ptr, s->edx, buf);
        curr_proc->fds[s->ebx].ptr += s->eax;
        return;
    }
}

// Spawns a new process from the file path specified by ebx.
// Returns eax = -1 on failure.
// Returns eax = new pid on success.
void sys_spawn_proc(struct syscall_registers *s) {
    if (!is_valid_user_addr(s->ebx)) {
        s->eax = -1;
        return;
    }
    // TODO: update base of this to be cwd
    struct inode *prog = get_inode_by_path(get_current_proc()->cwd, (char *) s->ebx);
    if (prog == 0) {
        s->eax = -1;
        return;
    }
    if (prog->type != FT_FILE) {
        s->eax = -1;
        return;
    }
    struct proc *new_proc = exec_helper(prog);
    if (new_proc == 0) {
        s->eax = -1;
    } else {
        struct proc *curr_proc = get_current_proc();
        new_proc->parent = curr_proc->pid;
        new_proc->cwd = curr_proc->cwd;
        // kprintf("Spawn %d\n", new_proc->pid);
        s->eax = new_proc->pid;
    }
}

void sys_getpid(struct syscall_registers *s) {
    s->eax = get_current_proc()->pid;
}

void sys_exit(struct syscall_registers *s) {
    // TODO: this might not work: sys_exit has a stack frame on the kernel stack, but that kernel stack gets cleaned up in cleanup_proc(). 
    // Instead, we should store kernel stack addr in the proc struct and free it (in scheduler()) once the process is killed.
    // TODO: in scheduler, we need to deal with killed processes that aren't being waited upon.
    cleanup_proc(get_current_proc());
    // kprintf("Kill %d\n", get_current_proc()->pid);
    __asm__ volatile ("int $0x20");
}

// If pid doesn't exist, fail
// If pid isn't child of current process, fail
// Otherwise, set state to waiting, set waiting_proc to pid
// Return eax = 0 on success, eax = -1 on failure.
void sys_wait(struct syscall_registers *s) {
    struct proc *curr_proc = get_current_proc();
    u32 pid = s->ebx;
    bool found = false;
    u32 i;
    for (i = 0; i < MAX_PROCS; i++) {
        if (ptable[i].pid == pid) {
            if (ptable[i].parent == get_current_proc()->pid) {
                found = true;
            }
            break;
        }
    }
    if (found) {
        curr_proc->status = WAITING_ON_PID;
        curr_proc->waiting_on = i;
        __asm__ volatile ("int $0x20");
        s->eax = 0;
    } else {
        s->eax = -1;
    }
}

u32 sbrk_temp_pgdir[PGDIR_LEN];
void sys_sbrk(struct syscall_registers *s) {
    struct proc *curr_proc = get_current_proc();

    // Round up brk_delta.
    i32 brk_delta = ((s->ebx + PGSIZE - 1) / PGSIZE) * PGSIZE;
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

    // Update brk value in proc struct.
    s->eax = curr_proc->brk;
    curr_proc->brk += brk_delta;
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

// Writes the full path of the pwd into the buf at ebx (whose length is ecx).
// Fails if the buf isn't large enough to accomodate the full path + null terminator.
void sys_pwd(struct syscall_registers *s) {
    // TODO: add "." and ".." entries in directory inode

    // Recurse up the tree and compute the length of the path
    struct proc *curr_proc = get_current_proc();
    u32 len = 0;
    struct inode *curr = curr_proc->cwd;
    while (strcmp(curr->name, FS_ROOT_PATH) != 0) {
        // strlen + path separator ('/')
        len += strlen(curr->name) + 1;
        curr = get_inode_at_idx(curr->parent);
    }
    // strlen + null terminator
    len += strlen(curr->name) + 1;
    if (len > s->ecx) {
        s->eax = -1;
        return;
    }
    // Then copy from the back
    curr = curr_proc->cwd;
    char *buf = (char *) s->ebx;
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
    s->eax = 0;
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
    // TODO: add permissions checking
    for (u32 i = 0; i < MAX_FDS; i++) {
        if (curr_proc->fds[i].status == FD_UNMAPPED) {
            if (target->type == FT_FILE) {
                curr_proc->fds[i].status = FD_REGULAR_FILE;
            } else {
                curr_proc->fds[i].status = FD_REGULAR_DIRECTORY;
            }
            curr_proc->fds[i].file = target;
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

    // TODO: add permissions checking
    if (target->type == FT_FILE) {
        curr_proc->fds[s->ebx].status = FD_REGULAR_FILE;
    } else {
        curr_proc->fds[s->ebx].status = FD_REGULAR_DIRECTORY;
    }
    curr_proc->fds[s->ebx].status = FD_REGULAR_FILE;
    curr_proc->fds[s->ebx].file = target;
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
        curr_proc->fds[s->ebx].file->data.file_data.size
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
    enum fd_status status = curr_proc->fds[s->ebx].status;
    if (status == FD_REGULAR_FILE) {
        s->eax = SYS_FTYPE_FILE;
        return;
    } else if (status == FD_REGULAR_DIRECTORY) {
        s->eax = SYS_FTYPE_DIR;
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
    if (curr_proc->fds[s->ebx].status != FD_REGULAR_FILE) {
        s->eax = -1;
        return;
    }
    if (!is_valid_user_addr(s->ecx)) {
        s->eax = -1;
        return;
    }
    struct file_info *info = (struct file_info *) s->ecx;
    info->size = curr_proc->fds[s->ebx].file->data.file_data.size;
    s->eax = 0;
}

// Writes dir info of fd ebx to (struct dir_info *) in ecx.
void sys_dir_info(struct syscall_registers *s) {
    if (s->ebx >= MAX_FDS) {
        s->eax = -1;
        return;
    }
    struct proc *curr_proc = get_current_proc();
    if (curr_proc->fds[s->ebx].status != FD_REGULAR_DIRECTORY) {
        s->eax = -1;
        return;
    }
    if (!is_valid_user_addr(s->ecx)) {
        s->eax = -1;
        return;
    }
    struct dir_info *info = (struct dir_info *) s->ecx;
    info->num_entries = curr_proc->fds[s->ebx].file->data.directory_data.num_entries;
    s->eax = 0;
}

// Writes dirent info of entry inside fd ebx, with offset provided in the (struct dirent_info *) ecx, to ecx.
void sys_dirent_info(struct syscall_registers *s) {
    if (s->ebx >= MAX_FDS) {
        s->eax = -1;
        return;
    }
    struct proc *curr_proc = get_current_proc();
    if (curr_proc->fds[s->ebx].status != FD_REGULAR_DIRECTORY) {
        s->eax = -1;
        return;
    }
    if (!is_valid_user_addr(s->ecx)) {
        s->eax = -1;
        return;
    }
    struct dirent_info *info = (struct dirent_info *) s->ecx;
    struct inode *dir = curr_proc->fds[s->ebx].file;
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
        default:
            kprintf("Invalid syscall code: %d\n", s->eax);
            break;
    }
}

