#ifndef ELF_DEFS_H
#define ELF_DEFS_H

#include "util.h"

#define ELF_MAGIC 0x464C457F

struct elf_header {
    u32 magic;
    char ident[12];
    u16 type;
    u16 machine;
    u32 version;
    u32 entry;
    u32 phoff;
    u32 shoff;
    u32 flags;
    u16 ehsize;
    u16 phentsize;
    u16 phnum;
    u16 shentsize;
    u16 shnum;
    u16 shstrndx;
};

struct program_header {
    u32 type;
    u32 offset;
    u32 vaddr;
    u32 paddr;
    u32 filesz;
    u32 memsz;
    u32 flags;
    u32 align;
};

#endif /* ELF_DEFS_H */
