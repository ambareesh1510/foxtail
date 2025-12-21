#include "cleanup.h"
#include "kprintf.h"
#include "paging.h"
#include "kstring.h"
#include "pgalloc.h"
#include "proc.h"
#include "util.h"

uint32_t cleanup_temp_pgdir[PGDIR_LEN];

void cleanup_proc(struct proc *proc) {
    // Release all pages.
    uint32_t *kernel_pgtbl = (uint32_t *) ((char *) kernel_id_pgtbl + HIGHER_HALF_BASE);
    uint32_t *temp_page_ptr = (uint32_t *) (HIGHER_HALF_BASE + PGSIZE * (PGDIR_LEN - 1));
    
    // Map cr3, then copy it to temp.
    kernel_pgtbl[PGDIR_LEN - 1] = proc->cr3 | 0x3;
    flush_tlb();
    memcpy((char *) cleanup_temp_pgdir, (char *) temp_page_ptr, PGSIZE);

    // Iterate until entry 768: only free user-space pages
    // TODO: remove magic number
    for (uint32_t pgdir_entry_idx = 0; pgdir_entry_idx < 768; pgdir_entry_idx++) {
        if ((cleanup_temp_pgdir[pgdir_entry_idx] & 0xFFFFF000) == 0) {
            continue;
        }
        kernel_pgtbl[PGDIR_LEN - 1] = (cleanup_temp_pgdir[pgdir_entry_idx] & 0xFFFFF000) | 0x3;
        flush_tlb();
        for (uint32_t pgtbl_entry_idx = 0; pgtbl_entry_idx < PGTBL_LEN; pgtbl_entry_idx++) {
            if ((temp_page_ptr[pgtbl_entry_idx] & 0xFFFFF000) == 0) {
                continue;
            }
            free_page(temp_page_ptr[pgtbl_entry_idx] / PGSIZE);
        }
        free_page(cleanup_temp_pgdir[pgdir_entry_idx] / PGSIZE);
    }
    free_page(proc->cr3);

    // Set proc status to killed.
    proc->status = KILLED;
    proc->present = false;
}
