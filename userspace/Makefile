# Userspace Makefile for URIX
# Licensed under MIT License - URIX project
ifeq ($(shell which x86_64-elf-gcc 2>/dev/null),)
CC = gcc
AS = as
LD = ld
else
CC = x86_64-elf-gcc
AS = x86_64-elf-as
LD = x86_64-elf-ld
endif
SRCDIR = src
INCDIR = include
BUILDDIR = build
CFLAGS = -m64 -ffreestanding -fno-pie -fno-stack-protector -nostdlib \
          -mno-red-zone -I$(INCDIR) -O2 -Wall -Wextra
ASFLAGS = --64
LDFLAGS = -nostdlib -static -T user.ld

LIB_SRCS  = liburix.c
PROG_SRCS = $(filter-out $(addprefix $(SRCDIR)/,$(LIB_SRCS)), $(wildcard $(SRCDIR)/*.c))
PROGRAMS = $(basename $(notdir $(PROG_SRCS)))

LIB_OBJS = $(BUILDDIR)/liburix.o $(BUILDDIR)/crt0.o

.PHONY: all clean
# Disable built-in rules
.SUFFIXES:

all: $(PROGRAMS)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/liburix.o: $(SRCDIR)/liburix.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/crt0.o: $(SRCDIR)/crt0.S | $(BUILDDIR)
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(PROGRAMS): %: $(BUILDDIR)/%.o $(LIB_OBJS)
	$(LD) $(LDFLAGS) -o $(BUILDDIR)/$@ $(BUILDDIR)/crt0.o $(BUILDDIR)/$@.o $(BUILDDIR)/liburix.o

clean:
	rm -rf $(BUILDDIR)
