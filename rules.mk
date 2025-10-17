# Makefile rules for URIX 64-bit kernel

CC = x86_64-elf-gcc
AS = x86_64-elf-as
LD = x86_64-elf-ld

# Directories
SRCDIR = src
BUILDDIR = build
ISODIR = build/iso

# Compute project root
PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

# Include flags (absolute)
INCLUDE_FLAGS := -I$(PROJECT_ROOT)/include

# Compiler flags
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -mno-red-zone -mcmodel=kernel $(INCLUDE_FLAGS)
ASFLAGS = --64
