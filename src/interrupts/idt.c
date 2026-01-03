/*
 * Licensed under MIT License - URIX project.
 * idt.c - Interrupt Descriptor Table implementation.
 * Responsibilities:
 *  - initialize and configure the IDT
 *  - install default exception handlers for CPU exceptions
 *  - provide interface for setting custom interrupt handlers
 *  - load IDT into the CPU
 * Notes:
 *  - IDT must be initialized before enabling interrupts
 *  - default handlers print debug info and halt
 *  - handlers preserve CPU state for debugging
 */

#include <interrupts/idt.h>
#include <interrupts/pic.h>
#include <process/process.h>
#include <memory/vmm.h>
#include <stdint.h>
#include <stddef.h>

// IDT table (256 entries for all interrupt vectors)
static struct idt_entry idt[256];

// IDT descriptor
static struct idt_ptr idtr;

// Forward declarations for exception handlers (defined in idt_handlers.S)
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

// Exception names for debugging
static const char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"};

void idt_set_gate(uint8_t vector, uint64_t handler, uint16_t selector, uint8_t type_attr)
{
    idt[vector].offset_low = handler & 0xFFFF;
    idt[vector].selector = selector;
    idt[vector].ist = 0; // No IST for now
    idt[vector].type_attr = type_attr;
    idt[vector].offset_mid = (handler >> 16) & 0xFFFF;
    idt[vector].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[vector].zero = 0;
}

void idt_init(void)
{
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

    // Install exception handlers (vectors 0-31)
    // Code segment selector is 0x08
    idt_set_gate(0, (uint64_t)isr0, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(1, (uint64_t)isr1, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(2, (uint64_t)isr2, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(3, (uint64_t)isr3, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(4, (uint64_t)isr4, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(5, (uint64_t)isr5, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(6, (uint64_t)isr6, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(7, (uint64_t)isr7, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(8, (uint64_t)isr8, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(9, (uint64_t)isr9, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(10, (uint64_t)isr10, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(11, (uint64_t)isr11, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(12, (uint64_t)isr12, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(13, (uint64_t)isr13, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(14, (uint64_t)isr14, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(15, (uint64_t)isr15, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(16, (uint64_t)isr16, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(17, (uint64_t)isr17, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(18, (uint64_t)isr18, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(19, (uint64_t)isr19, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(20, (uint64_t)isr20, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(21, (uint64_t)isr21, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(22, (uint64_t)isr22, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(23, (uint64_t)isr23, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(24, (uint64_t)isr24, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(25, (uint64_t)isr25, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(26, (uint64_t)isr26, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(27, (uint64_t)isr27, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(28, (uint64_t)isr28, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(29, (uint64_t)isr29, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(30, (uint64_t)isr30, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(31, (uint64_t)isr31, 0x08, IDT_GATE_INTERRUPT);

    // Install IRQ handlers (vectors 32-47)
    idt_set_gate(32, (uint64_t)isr32, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(33, (uint64_t)isr33, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(34, (uint64_t)isr34, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(35, (uint64_t)isr35, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(36, (uint64_t)isr36, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(37, (uint64_t)isr37, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(38, (uint64_t)isr38, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(39, (uint64_t)isr39, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(40, (uint64_t)isr40, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(41, (uint64_t)isr41, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(42, (uint64_t)isr42, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(43, (uint64_t)isr43, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(44, (uint64_t)isr44, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(45, (uint64_t)isr45, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(46, (uint64_t)isr46, 0x08, IDT_GATE_INTERRUPT);
    idt_set_gate(47, (uint64_t)isr47, 0x08, IDT_GATE_INTERRUPT);

    // Load the IDT
    __asm__ volatile("lidt %0" : : "m"(idtr));
}

// Structure to hold CPU state during exception
struct interrupt_frame
{
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

void irq_handler(struct interrupt_frame *frame)
{
    /* Calculate actual IRQ number (subtract 32 to get 0-15) */
    uint8_t irq = frame->int_no - 32;

    /* Handle specific IRQs */
    if (irq == 0) {
        /* Timer tick - update scheduler */
        process_timer_tick();
    }
    
    /* Send EOI to PIC */
    pic_send_eoi(irq);
}

// Common exception handler called from assembly stubs
void exception_handler(struct interrupt_frame *frame)
{
    extern int kprintf(const char *fmt, ...);

    kprintf("\n========== EXCEPTION ==========\n");
    kprintf("Exception: %s (vector %llu)\n",
            exception_messages[frame->int_no],
            frame->int_no);
    kprintf("Error Code: %llx\n", frame->err_code);
    kprintf("\nRegisters:\n");
    kprintf("RAX=%llx RBX=%llx RCX=%llx\n",
            frame->rax, frame->rbx, frame->rcx);
    kprintf("RDX=%llx RSI=%llx RDI=%llx\n",
            frame->rdx, frame->rsi, frame->rdi);
    kprintf("RBP=%llx RSP=%llx\n",
            frame->rbp, frame->rsp);
    kprintf("R8=%llx R9=%llx R10=%llx\n",
            frame->r8, frame->r9, frame->r10);
    kprintf("R11=%llx R12=%llx R13=%llx\n",
            frame->r11, frame->r12, frame->r13);
    kprintf("R14=%llx R15=%llx\n",
            frame->r14, frame->r15);
    kprintf("RIP=%llx CS =%llx\n",
            frame->rip, frame->cs);
    kprintf("RFLAGS=%llx\n", frame->rflags);
    kprintf("==============================\n");

    // Special handling for page fault
    if (frame->int_no == 14)
    {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        kprintf("Page Fault at address: %llx\n", cr2);
        kprintf("Error code bits:\n");
        kprintf("  Present: %d\n", frame->err_code & 0x1);
        kprintf("  Write: %d\n", (frame->err_code >> 1) & 0x1);
        kprintf("  User: %d\n", (frame->err_code >> 2) & 0x1);
        kprintf("  Reserved: %d\n", (frame->err_code >> 3) & 0x1);
        kprintf("  Instruction: %d\n", (frame->err_code >> 4) & 0x1);
    }

    // Halt the system
    kprintf("\nSystem halted.\n");
    while (1)
    {
        __asm__ volatile("cli; hlt");
    }
}

void idt_update_for_higher_half(void) {
    idtr.base = (uint64_t)phys_to_virt(virt_to_phys(&idt));
    
    // Reload the IDTR register
    __asm__ volatile("lidt %0" : : "m"(idtr));
}