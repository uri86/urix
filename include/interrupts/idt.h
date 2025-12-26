/*
 * Licensed under MIT License - URIX project.
 * idt.h - Interrupt Descriptor Table interface.
 * Responsibilities:
 *  - define IDT entry and descriptor structures
 *  - provide IDT initialization interface
 *  - expose exception handler registration
 * Notes:
 *  - IDT entries are 16 bytes in 64-bit mode
 *  - supports 256 interrupt vectors (0-255)
 *  - all handlers run at CPL=0 (kernel mode)
 */

#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// IDT entry structure (16 bytes in 64-bit mode)
struct idt_entry
{
    uint16_t offset_low;  // Offset bits 0-15
    uint16_t selector;    // Code segment selector
    uint8_t ist;          // Interrupt Stack Table offset (bits 0-2), rest reserved
    uint8_t type_attr;    // Type and attributes (P, DPL, S, Gate Type)
    uint16_t offset_mid;  // Offset bits 16-31
    uint32_t offset_high; // Offset bits 32-63
    uint32_t zero;        // Reserved, must be zero
} __attribute__((packed));

// IDT descriptor pointer structure
struct idt_ptr
{
    uint16_t limit; // Size of IDT - 1
    uint64_t base;  // Base address of IDT
} __attribute__((packed));

// IDT type attributes
#define IDT_GATE_INTERRUPT 0x8E // Present, DPL=0, Type=Interrupt Gate
#define IDT_GATE_TRAP 0x8F      // Present, DPL=0, Type=Trap Gate
#define IDT_GATE_TASK 0x85      // Present, DPL=0, Type=Task Gate

// Exception vectors (0-31 are reserved by Intel)
#define EXC_DIVIDE_ERROR 0
#define EXC_DEBUG 1
#define EXC_NMI 2
#define EXC_BREAKPOINT 3
#define EXC_OVERFLOW 4
#define EXC_BOUND_RANGE 5
#define EXC_INVALID_OPCODE 6
#define EXC_DEVICE_NOT_AVAILABLE 7
#define EXC_DOUBLE_FAULT 8
#define EXC_COPROCESSOR_SEGMENT 9
#define EXC_INVALID_TSS 10
#define EXC_SEGMENT_NOT_PRESENT 11
#define EXC_STACK_FAULT 12
#define EXC_GENERAL_PROTECTION 13
#define EXC_PAGE_FAULT 14
#define EXC_RESERVED_15 15
#define EXC_FPU_ERROR 16
#define EXC_ALIGNMENT_CHECK 17
#define EXC_MACHINE_CHECK 18
#define EXC_SIMD_FP_EXCEPTION 19
#define EXC_VIRTUALIZATION 20
#define EXC_CONTROL_PROTECTION 21

/**
 * Initialize the Interrupt Descriptor Table.
 * Must be called early in kernel initialization, before any interrupts occur.
 */
void idt_init(void);

/**
 * Set an IDT entry to point to a specific handler.
 *
 * vector - The interrupt vector number (0-255)
 * handler - Pointer to the interrupt handler function
 * selector - Code segment selector (typically 0x08)
 * type_attr - Type and attribute flags (use IDT_GATE_* constants)
 */
void idt_set_gate(uint8_t vector, uint64_t handler, uint16_t selector, uint8_t type_attr);

/**
 * Update the address stored inside the idtr pointer to reflect the virtual address.
 * Must be called before vmm_finish_init since after it runs,
 * the pointer is outdated and points to the physical memory.
 */
void idt_update_for_higher_half(void);

#endif // IDT_H