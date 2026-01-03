#include "paging.h"
#include "pgalloc.h"
#include "util.h"
#include "vga.h"

__attribute__((aligned(4096))) 
__attribute__ ((section(".boot.data")))
u32 kernel_pgdir[1024] = {0};
__attribute__((aligned(4096)))
__attribute__ ((section(".boot.data")))
u32 kernel_id_pgtbl[1024] = {0};

u32 *kernel_pgtbl = (u32 *) ((char *) kernel_id_pgtbl + HIGHER_HALF_BASE);
u32 *kernel_hh_pgdir = (u32 *) ((char *) kernel_pgdir + HIGHER_HALF_BASE);
u32 *temp_page_ptr = (u32 *) (HIGHER_HALF_BASE + PGSIZE * (PGDIR_LEN - 1));

// Must be called after framebuffer info is read from multiboot info.
void paging_setup(u32 *kernel_pgdir, u32 *kernel_id_pgtbl) {
  // Identity page the first megabyte.
  u32 addr = 0x0;
  for (u32 i = 0; i < PGTBL_LEN - 1; i++) {
    kernel_id_pgtbl[i] = (addr & 0xFFFFF000) | 0x3;
    addr += PGSIZE;
  }

  u32 kernel_pgdir_entry = ((u32)kernel_id_pgtbl & 0xfffff000) | 0x3;
  kernel_pgdir[0] = kernel_pgdir_entry;
  // We use 768 because we are mapping the kernel to address 0xC0000000; the
  // first 10 bits are the page directory index, which is equal to 768.
  kernel_pgdir[HIGHER_HALF_BASE >> 22] = kernel_pgdir_entry;

  if (graphics_mode.type == VGA_GRAPHICS_MODE) {
      u32 framebuffer_bytes = (graphics_mode.height * graphics_mode.width * graphics_mode.depth / 8);
      u32 pgtbl_alloc_size = PGSIZE * PGSIZE / sizeof(u32);
      u32 framebuffer_pgtbl_count = (framebuffer_bytes + pgtbl_alloc_size - 1) / pgtbl_alloc_size;
      addr = PAGE_ROUND_DOWN(graphics_mode.framebuffer);
      if (addr < 0x100000) {
          panic("Framebuffer address too low");
      }
      for (u32 i = 0; i < framebuffer_pgtbl_count; i++) {
          u32 new_page_phys_addr = alloc_page_low() * PGSIZE;
          u32 *new_page = (u32 *) new_page_phys_addr;
          for (u32 j = 0; j < PGSIZE / sizeof(u32); j++) {
              new_page[j] = addr | 0x3;
              addr += PGSIZE;
          }

          kernel_pgdir[(0xD0000000 >> 22) + i] = (new_page_phys_addr & 0xfffff000) | 0x3;
      }
  }


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
