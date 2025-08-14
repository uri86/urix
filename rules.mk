# Makefile rules for URIX 64-bit kernel

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
