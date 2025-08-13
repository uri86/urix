# Makefile for URIX 64-bit kernel

CC = x86_64-elf-gcc
AS = x86_64-elf-as
LD = x86_64-elf-ld

# Directories
SRCDIR = src
BUILDDIR = build
ISODIR = iso

# Compiler flags
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -mno-red-zone -mcmodel=kernel
ASFLAGS = --64
LDFLAGS = -T link.ld -nostdlib

# Source files
ASM_SOURCES = $(SRCDIR)/boot.S
C_SOURCES = $(SRCDIR)/kernel.c

# Object files
ASM_OBJECTS = $(ASM_SOURCES:$(SRCDIR)/%.S=$(BUILDDIR)/%.o)
C_OBJECTS = $(C_SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# Target kernel
KERNEL = $(BUILDDIR)/kernel.bin

# ISO file
ISO = urix.iso

.PHONY: all clean iso run install-deps

all: $(KERNEL)

# Create build directory
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Compile assembly files
$(BUILDDIR)/%.o: $(SRCDIR)/%.S | $(BUILDDIR)
	$(AS) $(ASFLAGS) -o $@ $<

# Compile C files
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Link kernel
$(KERNEL): $(OBJECTS)
	$(LD) -nostdlib -static -e _start -Ttext 0x100000 -o $@ $(OBJECTS)

# Create bootable ISO
iso: $(KERNEL)
	mkdir -p $(ISODIR)/boot/grub
	cp $(KERNEL) $(ISODIR)/boot/kernel.bin
	cp grub.cfg $(ISODIR)/boot/grub/grub.cfg
	i686-elf-grub-mkrescue -o $(ISO) $(ISODIR)

# Run in QEMU
run: iso
	qemu-system-x86_64 -cdrom $(ISO) -m 512M
# Debug with QEMU
debug: iso
	qemu-system-x86_64 -cdrom $(ISO) -m 512M -s -S

# Clean build files
clean:
	rm -rf $(BUILDDIR) $(ISO) $(ISODIR)

# Install dependencies on macOS
install-deps:
	@echo "Installing cross-compiler tools..."
	@echo "Make sure you have Homebrew installed, then run:"
	@echo "brew install x86_64-elf-gcc"
	@echo "brew install x86_64-elf-binutils"
	@echo "brew install i686-elf-grub"
	@echo "brew install qemu"
	@echo ""
	@echo "Note: You might need to use:"
	@echo "brew install --cask xquartz"
	@echo "if QEMU has display issues"

# Help
help:
	@echo "URIX Kernel Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build kernel binary"
	@echo "  iso          - Create bootable ISO"
	@echo "  run          - Run in QEMU"
	@echo "  debug        - Run in QEMU with GDB stub"
	@echo "  clean        - Clean build files"
	@echo "  install-deps - Show dependency installation commands"
	@echo "  help         - Show this help"