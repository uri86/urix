# Makefile for URIX kernel
include rules.mk

# Directories
LIBS = src/lib src/drivers

# Library object names (the .o they produce)
LIB_OBJS = $(foreach lib,$(LIBS),$(BUILDDIR)/$(notdir $(lib)).o)

# Source files
ASM_SOURCES = $(SRCDIR)/boot.S
C_SOURCES = $(SRCDIR)/kernel.c

# Object files
ASM_OBJECTS = $(ASM_SOURCES:$(SRCDIR)/%.S=$(BUILDDIR)/%.o)
C_OBJECTS   = $(C_SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
OBJECTS     = $(ASM_OBJECTS) $(C_OBJECTS) $(LIB_OBJS)

# Target kernel
KERNEL = $(BUILDDIR)/kernel.bin

# ISO
ISO = urix.iso

.PHONY: all clean iso run debug install-deps help $(LIBS)

all: $(KERNEL)

# Build each library using its own Makefile
$(LIB_OBJS):
	@for lib in $(LIBS); do \
		$(MAKE) -C $$lib CFLAGS="$(CFLAGS)"; \
		cp $$lib/build/lib.o $(BUILDDIR)/$$(basename $$lib).o; \
	done

# Create build dir
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Compile ASM
$(BUILDDIR)/%.o: $(SRCDIR)/%.S | $(BUILDDIR)
	$(AS) $(ASFLAGS) -o $@ $<

# Compile C
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Link kernel
$(KERNEL): $(OBJECTS)
	$(LD) -nostdlib -static -e _start -Ttext 0x100000 -o $@ $(OBJECTS)

# Create ISO
iso: $(KERNEL)
	mkdir -p $(ISODIR)/boot/grub
	cp $(KERNEL) $(ISODIR)/boot/kernel.bin
	cp grub.cfg $(ISODIR)/boot/grub/grub.cfg
	i686-elf-grub-mkrescue -o $(ISO) $(ISODIR)

run: iso
	qemu-system-x86_64 -cdrom $(ISO) -m 512M -d int -no-reboot -no-shutdown

debug: iso
	qemu-system-x86_64 -cdrom $(ISO) -m 512M -s -S

clean:
	rm -rf $(BUILDDIR) $(ISO) $(ISODIR)
	for lib in $(LIBS); do $(MAKE) -C $$lib clean; done
