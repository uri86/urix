/*
 * Licensed under MIT License - URIX project.
 * gdt.h - Global Descriptor Table interface.
 * Responsibilities:
 *  - define GDT entry structures
 *  - declare GDT initialization and loading functions
 *  - provide segment selector constants
 * Notes:
 *  - GDT defines memory segments and privilege levels
 *  - In long mode (64-bit), segmentation is mostly flat
 *  - We still need GDT for privilege levels (ring 0/3)
 *  - TSS (Task State Segment) is required for stack switching
 */

#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/*
 * Segment Selector Offsets
 *
 * These are offsets into the GDT (multiplied by 8 because each entry is 8 bytes).
 * Format: Index | TI | RPL
 *  - Index: Which GDT entry (bits 3-15)
 *  - TI: Table Indicator (bit 2) - 0 for GDT
 *  - RPL: Requested Privilege Level (bits 0-1) - 0 for kernel, 3 for user
 */
#define GDT_KERNEL_CODE 0x08 /* Kernel code segment */
#define GDT_KERNEL_DATA 0x10 /* Kernel data segment */
#define GDT_USER_CODE 0x18   /* User code segment */
#define GDT_USER_DATA 0x20   /* User data segment */
#define GDT_TSS 0x28         /* TSS segment */

/*
 * Access Byte Flags
 *
 * Structure: P | DPL (2 bits) | S | E | DC | RW | A
 *  - P (Present): Must be 1 for valid segment
 *  - DPL (Descriptor Privilege Level): 0=kernel, 3=user
 *  - S (System): 0=system segment (TSS), 1=code/data segment
 *  - E (Executable): 1=code segment, 0=data segment
 *  - DC (Direction/Conforming): For code: conforming bit, for data: direction
 *  - RW (Read/Write): For code: readable, for data: writable
 *  - A (Accessed): Set by CPU when segment is accessed
 */
#define GDT_ACCESS_PRESENT 0x80    /* Segment is present */
#define GDT_ACCESS_RING0 0x00      /* Ring 0 (kernel) */
#define GDT_ACCESS_RING3 0x60      /* Ring 3 (user) */
#define GDT_ACCESS_SYSTEM 0x00     /* System segment (TSS) */
#define GDT_ACCESS_CODESEG 0x10    /* Code/Data segment */
#define GDT_ACCESS_EXECUTABLE 0x08 /* Executable (code segment) */
#define GDT_ACCESS_READWRITE 0x02  /* Readable (code) / Writable (data) */

/*
 * Granularity Byte Flags
 *
 * Structure: G | DB | L | Reserved | Limit[19:16]
 *  - G (Granularity): 1=4KB blocks, 0=1 byte blocks
 *  - DB (Default operation size): 1=32-bit, 0=16-bit (ignored in long mode)
 *  - L (Long mode): 1=64-bit code segment, 0=compatibility mode
 *  - Limit[19:16]: Upper 4 bits of limit (ignored in 64-bit)
 */
#define GDT_GRAN_4K 0x80    /* 4KB granularity */
#define GDT_GRAN_BYTE 0x00  /* Byte granularity */
#define GDT_GRAN_32BIT 0x40 /* 32-bit protected mode */
#define GDT_GRAN_64BIT 0x20 /* 64-bit long mode */

/*
 * TSS Access Byte
 *
 * For TSS: P | DPL | 0 | Type (0b1001 = available 64-bit TSS)
 */
#define GDT_TSS_ACCESS 0x89 /* Present, Ring 0, Available TSS */


/*
 * GDT Entry Structure (8 bytes)
 *
 * In 64-bit mode, most fields are ignored but must be set correctly.
 * The important fields are:
 *  - Access byte: Present, DPL (privilege level), Type
 *  - Flags: Granularity, Size (D/B), Long mode (L)
 */
typedef struct gdt_entry
{
    uint16_t limit_low;  /* Limit bits 0-15 (ignored in 64-bit) */
    uint16_t base_low;   /* Base bits 0-15 (ignored in 64-bit) */
    uint8_t base_middle; /* Base bits 16-23 (ignored in 64-bit) */
    uint8_t access;      /* Access byte (P, DPL, S, Type) */
    uint8_t granularity; /* Flags + Limit bits 16-19 */
    uint8_t base_high;   /* Base bits 24-31 (ignored in 64-bit) */
} __attribute__((packed)) gdt_entry_t;

/*
 * TSS (Task State Segment) Entry - 16 bytes
 *
 * The TSS is special - it's a "system segment" that requires 2 GDT entries.
 * Used primarily for stack switching when changing privilege levels.
 */
typedef struct tss_entry
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
    uint32_t base_upper; /* Upper 32 bits of base (64-bit only) */
    uint32_t reserved;   /* Must be zero */
} __attribute__((packed)) tss_entry_t;

/*
 * GDT Pointer Structure
 *
 * Used by the LGDT instruction to load the GDT.
 * Limit = size of GDT - 1
 * Base = linear address of GDT
 */
typedef struct gdt_ptr
{
    uint16_t limit; /* Size of GDT - 1 */
    uint64_t base;  /* Address of GDT */
} __attribute__((packed)) gdt_ptr_t;

/*
 * TSS (Task State Segment) Structure
 */
typedef struct tss
{
    uint32_t reserved0;
    uint64_t rsp0; /* Stack pointer for ring 0 */
    uint64_t rsp1; /* Stack pointer for ring 1 (usually unused) */
    uint64_t rsp2; /* Stack pointer for ring 2 (usually unused) */
    uint64_t reserved1;
    uint64_t ist[7]; /* Interrupt Stack Table pointers */
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset; /* I/O Permission Bitmap offset */
} __attribute__((packed)) tss_t;


/**
 * gdt_init - Initialize the Global Descriptor Table
 *
 * Sets up a proper GDT with:
 *  - Null descriptor (required by x86)
 *  - Kernel code segment (64-bit, ring 0)
 *  - Kernel data segment (ring 0)
 *  - User code segment (64-bit, ring 3)
 *  - User data segment (ring 3)
 *  - TSS (Task State Segment)
 *
 * Also initializes the TSS and loads both GDT and TSS.
 */
void gdt_init(void);

/**
 * gdt_set_kernel_stack - Set the kernel stack in TSS
 *
 * stack_top: Top of kernel stack (physical address)
 *
 * Used when switching from user mode to kernel mode.
 * For now (ring 0 only), this is not critical, but we set it up properly.
 */
void gdt_set_kernel_stack(uint64_t stack_top);

/**
 * Update the address stored inside the gdt_ptr pointer to reflect the virtual address.
 * Must be called before vmm_finish_init since after it runs,
 * the pointer is outdated and points to the physical memory.
 */
void gdt_update_for_higher_half(void);

#endif /* GDT_H */