#ifndef PAGING_H
#define PAGING_H

#include "util.h"

#define PGDIR_LEN 1024
#define PGTBL_LEN PGDIR_LEN
#define PGSIZE 0x1000

#define PAGE_ROUND_DOWN(addr) (addr & ~(PGSIZE - 1))

#define HIGHER_HALF_BASE 0xC0000000

extern __attribute__((aligned(4096))) 
__attribute__ ((section(".boot.data")))
uint32_t kernel_pgdir[1024];
extern __attribute__((aligned(4096)))
__attribute__ ((section(".boot.data")))
uint32_t kernel_id_pgtbl[1024];

// void paging_setup();
void 
__attribute__ ((section(".boot.text")))
paging_setup(uint32_t *kernel_pgdir, uint32_t *kernel_id_pgtbl);

#endif /* PAGING_H */
