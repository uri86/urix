/*
 * Licensed under MIT License - URIX project.
 * idt.h - Interrupt Descriptor Table interface.
 * Responsibilities:
 *  - define IDT entry and descriptor structures
 *  - provide IDT initialization interface
 *  - expose exception handler registration
 */

#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// IDT entry structure
struct idt_entry
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
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

// CPU state pushed natively by the hardware during an exception
struct cpu_interrupt_frame
{
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed));

// CPU state pushed manually by isr_common_stub for generic handlers
struct interrupt_frame
{
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

// Exception handler callback type
typedef void (*exception_handler_t)(struct interrupt_frame *frame);

// Exception vectors
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
 */
void idt_init(void);

/**
 * Set an IDT entry to point to a specific handler.
 *
 * vector - The interrupt vector number (0-255)
 * handler - Pointer to the interrupt handler function
 * selector - Code segment selector
 * type_attr - Type and attribute flags
 */
void idt_set_gate(uint8_t vector, uint64_t handler, uint16_t selector, uint8_t type_attr);

/**
 * Update the address stored inside the idtr pointer to reflect the virtual address.
 */
void idt_update_for_higher_half(void);

void exception_handler(struct interrupt_frame *frame);

#endif // IDT_H