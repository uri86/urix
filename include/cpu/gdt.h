/*
 * Licensed under MIT License - URIX project.
 * gdt.h - Global Descriptor Table interface.
 * Responsibilities:
 *  - define GDT entry structures
 *  - declare GDT initialization and loading functions
 *  - provide segment selector constants
 * Notes:
 *  - GDT defines memory segments and privilege levels
 *  - TSS (Task State Segment) is required for stack switching
 */

#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/*
 * Segment Selector Offsets
 */
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE 0x18
#define GDT_USER_DATA 0x20
#define GDT_TSS 0x28

/*
 * Access Byte Flags
 *
 * Structure: P | DPL (2 bits) | S | E | DC | RW | A
 *  - P (Present): Must be 1 for valid segment
 *  - DPL (Descriptor Privilege Level)
 *  - S (System): 0=system segment, 1=code/data segment
 *  - E (Executable): 1=code segment, 0=data segment
 *  - DC (Direction/Conforming)
 *  - RW (Read/Write): For code: readable, for data: writable
 *  - A (Accessed): Set by CPU when segment is accessed
 */
#define GDT_ACCESS_PRESENT 0x80 
#define GDT_ACCESS_RING0 0x00
#define GDT_ACCESS_RING3 0x60
#define GDT_ACCESS_SYSTEM 0x00
#define GDT_ACCESS_CODESEG 0x10
#define GDT_ACCESS_EXECUTABLE 0x08
#define GDT_ACCESS_READWRITE 0x02

/*
 * Granularity Byte Flags
 *
 * Structure: G | DB | L | Reserved | Limit
 *  - G (Granularity): 1=4KB blocks, 0=1 byte blocks
 *  - DB (Default operation size)
 *  - L (Long mode), 1=64-bit code segment
 *  - Limit
 */
#define GDT_GRAN_4K 0x80
#define GDT_GRAN_BYTE 0x00
#define GDT_GRAN_32BIT 0x40
#define GDT_GRAN_64BIT 0x20

/*
 * TSS Access Byte
 *
 * For TSS: P | DPL | 0 | Type
 */
#define GDT_TSS_ACCESS 0x89 /* Present, Ring 0, Available TSS */


/*
 * GDT Entry Structure
 *
 *  - Access byte: Present, DPL (privilege level), Type
 *  - Flags: Granularity, Size (D/B), Long mode (L)
 */
typedef struct gdt_entry
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;      /* Access byte (P, DPL, S, Type) */
    uint8_t granularity; /* Flags + Limit bits 16-19 */
    uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

/*
 * TSS (Task State Segment) Entry
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
    uint16_t limit;
    uint64_t base;  /* Address of GDT */
} __attribute__((packed)) gdt_ptr_t;

/*
 * TSS (Task State Segment) Structure
 */
typedef struct tss
{
    uint32_t reserved0;
    uint64_t rsp0; /* Stack pointer for ring 0 */
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7]; /* Interrupt Stack Table pointers */
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed)) tss_t;


/**
 * gdt_init - Initialize the Global Descriptor Table
 *
 * Sets up a proper GDT with:
 *  - Null descriptor
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
 */
void gdt_set_kernel_stack(uint64_t stack_top);

/**
 * Update the address stored inside the gdt_ptr pointer to reflect the virtual address.
 */
void gdt_update_for_higher_half(void);

#endif /* GDT_H */