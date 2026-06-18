/*
 * Licensed under MIT License - URIX project.
 * idt.c - Interrupt Descriptor Table implementation.
 * Responsibilities:
 *  - initialize and configure the IDT
 *  - install default exception handlers for CPU exceptions
 *  - provide interface for setting custom interrupt handlers
 *  - load IDT into the CPU
 */

#include <interrupts/idt.h>
#include <interrupts/pic.h>
#include <process/process.h>
#include <drivers/keyboard.h>
#include <memory/vmm.h>
#include <lib/panic.h>
#include <lib/print.h>
#include <lib/utils.h>
#include <stdint.h>
#include <stddef.h>

// IDT table
static struct idt_entry idt[256];

// IDT descriptor
static struct idt_ptr idtr;

// Forward declarations for exception handlers
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

extern void isr32(void);
extern void isr33(void);
extern void isr34(void);
extern void isr35(void);
extern void isr36(void);
extern void isr37(void);
extern void isr38(void);
extern void isr39(void);
extern void isr40(void);
extern void isr41(void);
extern void isr42(void);
extern void isr43(void);
extern void isr44(void);
extern void isr45(void);
extern void isr46(void);
extern void isr47(void);

__attribute__((interrupt)) static void divide_by_zero_exception_handler(struct cpu_interrupt_frame *frame);
__attribute__((interrupt)) static void invalid_opcode_exception_handler(struct cpu_interrupt_frame *frame);
__attribute__((interrupt)) static void general_protection_fault_handler(struct cpu_interrupt_frame *frame, uint64_t error_code);
__attribute__((interrupt)) static void page_fault_exception_handler(struct cpu_interrupt_frame *frame, uint64_t error_code);
static const void *isr_vectors[48] = {
    (void *)divide_by_zero_exception_handler, isr1, isr2, isr3,
    isr4, isr5, (void *)invalid_opcode_exception_handler, isr7,
    isr8, isr9, isr10, isr11,
    isr12, (void *)general_protection_fault_handler, (void *)page_fault_exception_handler, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
    isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39,
    isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47};

void idt_set_gate(uint8_t vector, uint64_t handler, uint16_t selector, uint8_t type_attr)
{
    idt[vector].offset_low = handler & 0xFFFF;
    idt[vector].selector = selector;
    idt[vector].ist = 0;
    idt[vector].type_attr = type_attr;
    idt[vector].offset_mid = (handler >> 16) & 0xFFFF;
    idt[vector].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[vector].zero = 0;
}

void idt_init(void)
{
    // Ensure interrupts are disabled during table setup.
    __asm__ volatile("cli");

    // Set up IDT pointer
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)&idt;

    // Clear the entire IDT
    for (int i = 0; i < 256; i++)
    {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].ist = 0;
        idt[i].type_attr = 0;
        idt[i].offset_mid = 0;
        idt[i].offset_high = 0;
        idt[i].zero = 0;
    }

    // Install exception and IRQ handlers using the lookup table
    for (int i = 0; i < 48; i++)
    {
        idt_set_gate(i, (uint64_t)isr_vectors[i], 0x08, IDT_GATE_INTERRUPT);
    }

    // Load the IDT
    __asm__ volatile("lidt %0" : : "m"(idtr));

    debug_kprintf("Debug: IDT loaded, vectors 0-47 mapped\n");
}

void irq_handler(struct interrupt_frame *frame)
{

    /* Calculate actual IRQ number (subtract 32 to get 0-15) */
    uint8_t irq = frame->int_no - 32;

    /* Send EOI to PIC */
    pic_send_eoi(irq);

    /* Handle specific IRQs */
    if (irq == 0)
    {
        /* Timer tick - update scheduler */
        process_timer_tick();
    }
    else if (irq == 1)
    {
        /* Keyboard interrupt */
        keyboard_interrupt_handler();
    }
}

__attribute__((interrupt))
static void invalid_opcode_exception_handler(struct cpu_interrupt_frame *frame)
{
    process_t *current = process_get_current();
    kprintf("[EXCEPTION] Invalid Opcode at RIP 0x%llx, vector 6\n", frame->rip);
    if (current && current->privilege == PROCESS_USER)
    {
        kprintf("[EXCEPTION] Killing user process %u (%s) due to invalid opcode\n", current->pid, current->name);
        process_exit(6);
    }
    PANIC("Kernel Invalid Opcode");
}

__attribute__((interrupt))
static void general_protection_fault_handler(struct cpu_interrupt_frame *frame, uint64_t error_code)
{
    process_t *current = process_get_current();
    kprintf("[EXCEPTION] General Protection Fault at RIP 0x%llx, code 0x%llx\n", frame->rip, error_code);
    if (current && current->privilege == PROCESS_USER)
    {
        kprintf("[EXCEPTION] Killing user process %u (%s) due to GPF\n", current->pid, current->name);
        process_exit(13);
    }
    PANIC("Kernel General Protection Fault");
}

__attribute__((interrupt)) static void divide_by_zero_exception_handler(struct cpu_interrupt_frame *frame)
{
    process_t *current = process_get_current();
    kprintf("[EXCEPTION] Divide by zero at RIP 0x%llx, vector 0\n", frame->rip);

    if (current && current->privilege == PROCESS_USER)
    {
        kprintf("[EXCEPTION] Killing user process %u (%s) with status 1\n", current->pid, current->name);
        process_exit(1);
    }

    PANIC("Kernel divide by zero");
}

__attribute__((interrupt))
static void page_fault_exception_handler(struct cpu_interrupt_frame *frame, uint64_t error_code)
{
    process_t *current = process_get_current();
    uint64_t cr2;
    // Read the faulting address from CR2
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

   kprintf("[EXCEPTION] Page fault at address 0x%llx, RIP 0x%llx, code 0x%llx\n", cr2, frame->rip, error_code);
    if (current && current->privilege == PROCESS_USER)
    {
        kprintf("[EXCEPTION] Killing user process %u (%s) due to page fault\n", current->pid, current->name);
        process_exit(2);
    }

    PANIC("Kernel Page Fault");
}

void exception_handler(struct interrupt_frame *frame)
{

    kprintf("\n========== EXCEPTION ==========\n");
    kprintf("Exception: vector %llu\n", frame->int_no);
    kprintf("Error Code: 0x%llx\n", frame->err_code);
    kprintf("\nRegisters:\n");
    kprintf("RAX=0x%llx RBX=0x%llx RCX=0x%llx\n", frame->rax, frame->rbx, frame->rcx);
    kprintf("RDX=0x%llx RSI=0x%llx RDI=0x%llx\n", frame->rdx, frame->rsi, frame->rdi);
    kprintf("RBP=0x%llx RSP=0x%llx\n", frame->rbp, frame->rsp);
    kprintf("R8=0x%llx R9=0x%llx R10=0x%llx\n", frame->r8, frame->r9, frame->r10);
    kprintf("R11=0x%llx R12=0x%llx R13=0x%llx\n", frame->r11, frame->r12, frame->r13);
    kprintf("R14=0x%llx R15=0x%llx\n", frame->r14, frame->r15);
    kprintf("RIP=0x%llx CS =0x%llx\n", frame->rip, frame->cs);
    kprintf("RFLAGS=0x%llx\n", frame->rflags);
    kprintf("==============================\n");
    kprintf("\nSystem halted.\n");
    halt();
}

void idt_update_for_higher_half(void)
{
    idtr.base = (uint64_t)phys_to_virt(virt_to_phys(&idt));

    // Reload the IDTR register
    __asm__ volatile("lidt %0" : : "m"(idtr));
}