# Makefile for URIX kernel
include rules.mk

# Directories
LIBS = src/lib src/drivers src/memory src/interrupts src/cpu src/tests src/process src/fs

# Library object names (the .o they produce)
LIB_OBJS = $(foreach lib,$(LIBS),$(BUILDDIR)/$(notdir $(lib)).o)

# Source files
ASM_SOURCES = $(SRCDIR)/boot.S
C_SOURCES   = $(SRCDIR)/kernel.c

# Object files
ASM_OBJECTS = $(ASM_SOURCES:$(SRCDIR)/%.S=$(BUILDDIR)/%.o)
C_OBJECTS   = $(C_SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
OBJECTS     = $(ASM_OBJECTS) $(C_OBJECTS) $(LIB_OBJS)

# Target kernel
KERNEL = $(BUILDDIR)/kernel.bin

# ISO
ISO = urix.iso

.PHONY: all clean iso run run-qemu rerun rerun-y $(LIBS)

# Default target
all: $(KERNEL)

# Build libraries
$(LIB_OBJS):
	@for lib in $(LIBS); do \
		$(MAKE) -C $$lib CFLAGS="$(CFLAGS)"; \
		cp $$lib/build/lib.o $(BUILDDIR)/$$(basename $$lib).o; \
	done

# Build directory
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Compile ASM
$(BUILDDIR)/%.o: $(SRCDIR)/%.S | $(BUILDDIR)
	$(AS) $(ASFLAGS) -o $@ $<

# Compile C
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(KERNEL): $(OBJECTS)
	$(LD) -nostdlib -static -T linker.ld -o $@ $(OBJECTS)

iso: $(KERNEL)
	mkdir -p $(ISODIR)/boot/grub
	cp $(KERNEL) $(ISODIR)/boot/kernel.bin
	cp grub.cfg $(ISODIR)/boot/grub/grub.cfg
	i686-elf-grub-mkrescue -o $(ISO) $(ISODIR)

run-qemu:
	qemu-system-x86_64 \
		-m size=4096M \
		-d int \
		-no-reboot \
		-no-shutdown \
		-drive file=disk.img,format=raw,if=ide,index=0 \
		-cdrom $(ISO)

run: iso
	$(MAKE) run-qemu

rerun:
	@if [ "$(REBUILD)" = "1" ]; then \
		$(MAKE) clean run; \
	else \
		printf "Recompile before running? [y/N]: "; \
		read ans; \
		if [ "$$ans" = "y" ] || [ "$$ans" = "Y" ]; then \
			$(MAKE) clean run; \
		else \
			$(MAKE) run-qemu; \
		fi; \
	fi

# Forced rebuild rerun
rerun-y:
	$(MAKE) rerun REBUILD=1

clean:
	rm -rf $(BUILDDIR) $(ISO) $(ISODIR)
	for lib in $(LIBS); do $(MAKE) -C $$lib clean; done
