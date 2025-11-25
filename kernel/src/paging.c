#include "paging.h"
#include "util.h"

// Address that the kernel is mapped to in physical memory.

void 
// __attribute__ ((section(".boot")))
paging_setup(uint32_t *kernel_pgdir, uint32_t *kernel_id_pgtbl) {
  // Identity page the first megabyte.
  uint32_t addr = 0x0;
  for (uint32_t i = 0; i < PGTBL_LEN; i++) {
    kernel_id_pgtbl[i] = (addr & 0xFFFFF000) | 0x3;
    addr += PGSIZE;
  }

  uint32_t kernel_pgdir_entry = ((uint32_t)kernel_id_pgtbl & 0xfffff000) | 0x3;
  kernel_pgdir[0] = kernel_pgdir_entry;
  // We use 768 because we are mapping the kernel to address 0xC0000000; the
  // first 10 bits are the page directory index, which is equal to 768.
  kernel_pgdir[HIGHER_HALF_BASE >> 22] = kernel_pgdir_entry;

  // Enable paging.
  __asm__ volatile(
                   "mov %0, %%eax\n"
                   "mov %%eax, %%cr3\n"

                   "mov %%cr0, %%eax\n"
                   "orl $0x80000001, %%eax\n"
                   "mov %%eax, %%cr0\n"
                   :
                   : "r"(kernel_pgdir)
                   : "%eax");
}
