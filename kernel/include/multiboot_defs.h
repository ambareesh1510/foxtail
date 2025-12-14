#ifndef MULTIBOOT_DEFS_H
#define MULTIBOOT_DEFS_H

#include "util.h"

struct aout_symbol_table {
 uint32_t tabsize;
 uint32_t strsize;
 uint32_t addr;
 uint32_t reserved;
};

/* The section header table for ELF. */
struct elf_section_header_table {
 uint32_t num;
 uint32_t size;
 uint32_t addr;
 uint32_t shndx;
};

struct multiboot_info {
 uint32_t flags;
 uint32_t mem_lower;
 uint32_t mem_upper;
 uint32_t boot_device;
 uint32_t cmdline;
 uint32_t mods_count;
 uint32_t mods_addr;
 union {
   struct aout_symbol_table aout_sym;
   struct elf_section_header_table elf_sec;
 } u;
 uint32_t mmap_length;
 uint32_t mmap_addr;
};

struct memory_map
{
 uint32_t size;
 uint32_t base_addr_low;
 uint32_t base_addr_high;
 uint32_t length_low;
 uint32_t length_high;
 uint32_t type;
};

#endif /* MULTIBOOT_DEFS_H */
