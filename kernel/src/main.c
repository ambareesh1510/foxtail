#include "interrupt.h"
#include "gdt.h"
#include "vga.h"
#include "paging.h"
#include "kprintf.h"
#include "fs.h"
#include "util.h"

__attribute__((aligned(4096))) 
__attribute__ ((section(".boot.data")))
uint32_t kernel_pgdir[1024] = {0};
__attribute__((aligned(4096)))
__attribute__ ((section(".boot.data")))
uint32_t kernel_id_pgtbl[1024] = {0};

void higher_half_entry();

void 
__attribute__ ((section(".boot.text")))
kernel_main(void) {
    paging_setup(kernel_pgdir, kernel_id_pgtbl);

    // Increment stack pointer and base pointer so they point to higher half
    // addresses. We need this so that we can use the stack once the lower half
    // mapping is invalidated.
    __asm__ volatile (
        "add $0xC0000000, %esp\n"
        "add $0xC0000000, %ebp\n"
    );

    higher_half_entry();
}

struct inode *get_inode_at_idx(uint32_t idx) {
    return (struct inode *) (fs + idx * sizeof(struct inode));
}

uint32_t strlen(const char *str) {
    uint32_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

uint32_t strcmp(const char *a, const char *b) {
    int i = 0;
    for (;;) {
        if (a[i] == '\0' || b[i] == '\0') {
            if (a[i] == '\0' && b[i] == '\0') {
                return 0;
            }
            return 1;
        }
        if (a[i] != b[i]) {
            return 1;
        }
        i++;
    }
}

struct inode *get_inode_by_path(
    struct inode *base,
    const char *path
) {
    if (base == 0) {
        return base;
    }
    if (path[0] == '/') {
        path++;
    }
    if (strlen(path) == 0) {
        return base;
    }
    if (base->type == FT_FILE) {
        return 0;
    }
    char path_buf[FILENAME_MAX_LEN] = {0};
    uint32_t i = 0;
    for (; path[i] != '/' && path[i] != '\0'; i++) {
        path_buf[i] = path[i];
    }
    path_buf[i] = '\0';
    struct inode *next = 0;
    for (uint32_t j = 0; j < base->data.directory_data.num_entries; j++) {
        struct inode *temp = get_inode_at_idx(base->data.directory_data.direct_files[j]);
        if (strcmp(temp->name, path_buf) == 0) {
            next = temp;
            break;
        }
    }
    return get_inode_by_path(next, path + i);
}

struct inode *get_root_inode() {
    return get_inode_at_idx(0);
}

void tree(struct inode *base, uint32_t depth) {
    if (base == 0) {
        kprintf("Invalid inode\n");
        return;
    }
    if (depth > 2) {
        return;
    }
    if (depth > 0) {
        for (uint32_t i = 0; i < depth - 1; i++) {
            kprintf("   ");
        }
        kprintf("|- ");
    }
    if (base->type == FT_FILE) {
        kprintf("%s\n", base->name);
    } else {
        kprintf("%s/\n", base->name);
        for (uint32_t j = 0; j < base->data.directory_data.num_entries; j++) {
            struct inode *next = get_inode_at_idx(base->data.directory_data.direct_files[j]);
            tree(next, depth + 1);
        }
    }
}

void
higher_half_entry() {
    // Unmap the identity mapping of the lower half.
    kernel_pgdir[0] = 0;
    __asm__ volatile ("invlpg [0]");

    vga_clear();

    // We need to set the GDT to be able to use segment selectors in the higher
    // half.
    gdt_load();
    idt_load();
    // TODO: this doesn't work if "hi/" doesn't have the trailing slash
    tree(get_inode_by_path(get_root_inode(), "hi"), 0);
    // tree(get_root_inode(), 0);

    // for (;;) {
    //     kprintf("Ticks: %d, kb: %x\n", ticks, (uint32_t) (unsigned char) kb_char);
    // }

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
