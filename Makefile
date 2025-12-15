
SRC_DIR = kernel/src
INCLUDE_DIR = kernel/include
OBJ_DIR = bin

SRCS_C = $(wildcard $(SRC_DIR)/*.c)
SRCS_S = $(wildcard $(SRC_DIR)/*.s)
SRCS = $(SRCS_C) $(SRCS_S)
OBJS = $(patsubst $(SRC_DIR)/%, $(OBJ_DIR)/%, $(SRCS_C:.c=.o) $(SRCS_S:.s=.o))

KERNEL = kernel.elf

ISO_DIR = isodir
ISO = os.iso

CC = clang
LD = ld.lld
CFLAGS = -target i386-elf -std=c23 -m32 -ffreestanding -fno-builtin -O2 -Wall -Wextra -Wpedantic -nostdlib -mno-sse -I $(INCLUDE_DIR)
LDFLAGS = -m elf_i386 -nostdlib -T link.ld

.PHONY: all clean run iso_dir prepare_iso prepare

all: prepare fs $(ISO)

echo:
	echo $(OBJS)

# Rule for assembling .s files into .o files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.s
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for compiling .c files into .o files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): fs $(OBJS) link.ld
	$(LD) $(LDFLAGS) -o $(KERNEL) $(OBJS)

iso_dir: $(KERNEL)
	rm -rf $(ISO_DIR)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/$(KERNEL)
	cp grub.cfg $(ISO_DIR)/boot/grub/grub.cfg

$(ISO): iso_dir
	grub-mkrescue -o $(ISO) $(ISO_DIR)

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -m 512

kernel: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -m 1024M -d int,cpu_reset

clean:
	rm -f $(KERNEL) $(ISO)
	rm -rf $(ISO_DIR)
	rm -rf $(OBJ_DIR)
	rm -r fs.bin

prepare:
	mkdir -p $(OBJ_DIR)

fs:
	rm -f fs.bin
	mkdir -p mkfs/bin
	$(CC) -std=c23 -Ikernel/include/ mkfs/src/main.c -o mkfs/bin/mkfs -D_DEFAULT_SOURCE
	mkfs/bin/mkfs
