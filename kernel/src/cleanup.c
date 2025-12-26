#include "cleanup.h"
#include "fs.h"
#include "kprintf.h"
#include "paging.h"
#include "kstring.h"
#include "pgalloc.h"
#include "proc.h"
#include "syscall.h"
#include "util.h"

u32 cleanup_temp_pgdir[PGDIR_LEN];

void cleanup_proc(struct proc *proc) {
    // Release all pages.
    
    // Map cr3, then copy it to temp.
    kernel_pgtbl[PGDIR_LEN - 1] = proc->cr3 | 0x3;
    flush_tlb();
    memcpy((char *) cleanup_temp_pgdir, (char *) temp_page_ptr, PGSIZE);

    // Iterate until entry 768: only free user-space pages
    // TODO: remove magic number
    for (u32 pgdir_entry_idx = 0; pgdir_entry_idx < 768; pgdir_entry_idx++) {
        if ((cleanup_temp_pgdir[pgdir_entry_idx] & 0xFFFFF000) == 0) {
            continue;
        }
        kernel_pgtbl[PGDIR_LEN - 1] = (cleanup_temp_pgdir[pgdir_entry_idx] & 0xFFFFF000) | 0x3;
        flush_tlb();
        for (u32 pgtbl_entry_idx = 0; pgtbl_entry_idx < PGTBL_LEN; pgtbl_entry_idx++) {
            if ((temp_page_ptr[pgtbl_entry_idx] & 0xFFFFF000) == 0) {
                continue;
            }
            free_page(temp_page_ptr[pgtbl_entry_idx] / PGSIZE);
        }
        free_page(cleanup_temp_pgdir[pgdir_entry_idx] / PGSIZE);
    }
    free_page(proc->cr3);

    for (u32 i = 0; i < MAX_FDS; i++) {
        enum fd_status status = proc->fds[i].status;
        if (status == FD_REGULAR_FILE || status == FD_REGULAR_DIRECTORY) {
            release_inode(proc->fds[i].data.file);
        } else if (status == FD_PIPE) {
            pipe_data[proc->fds[i].data.pipe_idx].num_refs--;
        }
    }

    // Set proc status to killed.
    proc->status = KILLED;
    proc->present = false;
    proc->exit_code = FAULT_EXIT_CODE;
}
