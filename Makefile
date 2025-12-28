# Shared definitions
SHARED_INCLUDE_DIR = shared/

# Kernel definitions
KERNEL_SRC_DIR = kernel/src
KERNEL_INCLUDE_DIR = kernel/include
KERNEL_OBJ_DIR = kernel/bin

KERNEL_SRCS_C = $(wildcard $(KERNEL_SRC_DIR)/*.c)
KERNEL_SRCS_S = $(wildcard $(KERNEL_SRC_DIR)/*.s)
KERNEL_OBJS = $(patsubst $(KERNEL_SRC_DIR)/%, $(KERNEL_OBJ_DIR)/%, $(KERNEL_SRCS_C:.c=.o) $(KERNEL_SRCS_S:.s=.o))

KERNEL = kernel.elf

ISO_DIR = isodir
ISO = os.iso

# User program definitions
USER_SRC_DIR = user/src
USER_INCLUDE_DIR = user/include
USER_OBJ_DIR = user/obj
USER_EXE_DIR = user/bin

USER_SRCS_C = $(wildcard $(USER_SRC_DIR)/*.c)
USER_OBJS = $(USER_SRCS_C:$(USER_SRC_DIR)/%.c=$(USER_OBJ_DIR)/%.o)
USER_EXES = $(USER_SRCS_C:$(USER_SRC_DIR)/%.c=$(USER_EXE_DIR)/%)

# mkfs definitions
MKFS_FS_DIR = mkfs/fs
MKFS_SRC_DIR = mkfs/src
MKFS_BIN_DIR = mkfs/bin
MKFS_FS_OUT_FILE = fs.bin

# Compiler/linker options
CC = clang
LD = ld.lld
CFLAGS = -target i386-elf -std=c23 -m32 -ffreestanding -fno-builtin -O2 -Wall -Wextra -Wpedantic -nostdlib -mno-sse -I $(KERNEL_INCLUDE_DIR) -I $(SHARED_INCLUDE_DIR) -g
USER_CFLAGS = -target i386-elf -std=c23 -m32 -ffreestanding -fno-builtin -O2 -Wall -Wextra -Wpedantic -nostdlib -mno-sse -I $(USER_INCLUDE_DIR) -I $(SHARED_INCLUDE_DIR)
LDFLAGS = -m elf_i386 -nostdlib -T link.ld
USER_LDFLAGS = -m elf_i386 -nostdlib -T user.ld --strip-all

.PHONY: all clean run iso_dir prepare_iso prepare

all: prepare fs $(ISO)

echo:
	echo $(KERNEL_OBJS)

# Rule for assembling .s files into .o files
$(KERNEL_OBJ_DIR)/%.o: $(KERNEL_SRC_DIR)/%.s
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for compiling .c files into .o files
$(KERNEL_OBJ_DIR)/%.o: $(KERNEL_SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): fs $(KERNEL_OBJS) link.ld
	$(LD) $(LDFLAGS) -o $(KERNEL) $(KERNEL_OBJS)

iso_dir: $(KERNEL)
	rm -rf $(ISO_DIR)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/$(KERNEL)
	cp grub.cfg $(ISO_DIR)/boot/grub/grub.cfg

$(ISO): iso_dir
	grub-mkrescue -o $(ISO) $(ISO_DIR) -d /usr/lib/grub/i386-pc

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -m 512M -d int,cpu_reset --enable-kvm -no-reboot -no-shutdown

gdb: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -m 1024M -d int,cpu_reset --enable-kvm -no-reboot -no-shutdown -s -S

kernel: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -m 1024M -d int,cpu_reset -no-reboot -no-shutdown

clean:
	rm -f $(KERNEL) $(ISO)
	rm -rf $(ISO_DIR)
	rm -rf $(KERNEL_OBJ_DIR)
	rm -rf $(USER_OBJ_DIR) $(USER_EXE_DIR)
	rm -f $(MKFS_FS_OUT_FILE)

prepare:
	mkdir -p $(KERNEL_OBJ_DIR)

prepare_user:
	mkdir -p $(USER_OBJ_DIR)
	mkdir -p $(USER_EXE_DIR)

user: prepare_user $(USER_EXES)

$(USER_EXE_DIR)/%: $(USER_OBJ_DIR)/%.o
	$(LD) $(USER_LDFLAGS) -o $@ $<

$(USER_OBJ_DIR)/%.o: $(USER_SRC_DIR)/%.c
	$(CC) $(USER_CFLAGS) -c $< -o $@

fs: user
	rm -f $(MKFS_FS_OUT_FILE)
	mkdir -p $(MKFS_FS_DIR)
	cp $(USER_EXE_DIR)/* $(MKFS_FS_DIR)
	mkdir -p $(MKFS_BIN_DIR)
	$(CC) -std=c23 -I$(KERNEL_INCLUDE_DIR) $(MKFS_SRC_DIR)/main.c -o $(MKFS_BIN_DIR)/mkfs -D_DEFAULT_SOURCE
	$(MKFS_BIN_DIR)/mkfs
