# URIX Root Makefile
include rules.mk

LIBS = libc/src src/lib src/drivers src/memory src/interrupts src/cpu src/tests src/process src/fs src/syscall

LIB_OBJS = $(foreach lib,$(LIBS),$(BUILDDIR)/$(notdir $(lib)).o)

ASM_SOURCES = $(SRCDIR)/boot.S
C_SOURCES   = $(SRCDIR)/kernel.c $(SRCDIR)/embedded_programs.c

ASM_OBJECTS = $(ASM_SOURCES:$(SRCDIR)/%.S=$(BUILDDIR)/%.o)
C_OBJECTS   = $(C_SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
OBJECTS     = $(ASM_OBJECTS) $(C_OBJECTS) $(LIB_OBJS)

KERNEL = $(BUILDDIR)/kernel.bin
ISO = urix.iso


USERSPACE_DIR = userspace
USERSPACE_BUILD = $(USERSPACE_DIR)/build

EMBEDDED_C = $(SRCDIR)/embedded_programs.c
EMBEDDED_H = include/embedded_programs.h

.PHONY: all clean iso run $(LIBS) userspace qemu

all: $(KERNEL)

$(USERSPACE_BINS):
	$(MAKE) -C $(USERSPACE_DIR)

userspace:
	$(MAKE) -C $(USERSPACE_DIR)

$(EMBEDDED_C) $(EMBEDDED_H): userspace
	cd $(USERSPACE_DIR) && ./gen_embedded.sh
	cp $(USERSPACE_DIR)/embedded_programs.c $(SRCDIR)/
	cp $(USERSPACE_DIR)/embedded_programs.h include/

$(LIB_OBJS):
	@for lib in $(LIBS); do \
		$(MAKE) -C $$lib CFLAGS="$(CFLAGS)"; \
		cp $$lib/build/lib.o $(BUILDDIR)/$$(basename $$lib).o; \
	done

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: $(SRCDIR)/%.S | $(BUILDDIR)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Ensure embedded gets generated first
$(BUILDDIR)/embedded_programs.o: $(EMBEDDED_C)

$(KERNEL): $(EMBEDDED_C) $(OBJECTS)
	$(LD) -nostdlib -static -T linker.ld -o $@ $(OBJECTS)


iso: $(KERNEL)
	@echo "[URIX] Building ISO..."
	mkdir -p $(ISODIR)/boot/grub
	cp $(KERNEL) $(ISODIR)/boot/kernel.bin
	cp grub.cfg $(ISODIR)/boot/grub/grub.cfg
	i686-elf-grub-mkrescue -o $(ISO) $(ISODIR)

qemu:
	@echo "[URIX] Launching QEMU..."
	qemu-system-x86_64 \
		-m size=4096M \
		-d int \
		-no-reboot \
		-no-shutdown \
		-drive file=disk.img,format=raw,if=ide,index=0 \
		-cdrom $(ISO)

run: iso qemu


clean:
	rm -rf $(BUILDDIR) $(ISO)
	for lib in $(LIBS); do $(MAKE) -C $$lib clean; done
	$(MAKE) -C $(USERSPACE_DIR) clean
	rm -rf **/embedded_programs.*
