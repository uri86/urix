# idt_handlers.S - Low-level interrupt handler stubs for URIX
# Licensed under MIT License - URIX project.
#
# Responsibilities:
#  - save CPU state on interrupt entry
#  - push interrupt number and error code
#  - call high-level C exception handler
#  - restore CPU state on return
#
# Notes:
#  - some exceptions push error codes, others don't
#  - we push dummy error code (0) for consistency
#  - all registers are preserved across interrupts

.section .text
.code64

# Macro for ISRs that don't push error codes
.macro ISR_NOERR num
.global isr\num
.type isr\num, @function
isr\num:
    pushq $0                  # Push dummy error code
    pushq $\num               # Push interrupt number
    jmp isr_common_stub
.endm

# Macro for ISRs that push error codes
.macro ISR_ERR num
.global isr\num
.type isr\num, @function
isr\num:
    pushq $\num               # Push interrupt number (error code already pushed)
    jmp isr_common_stub
.endm

# Define all 32 exception handlers
ISR_NOERR 0     # Division By Zero
ISR_NOERR 1     # Debug
ISR_NOERR 2     # Non Maskable Interrupt
ISR_NOERR 3     # Breakpoint
ISR_NOERR 4     # Overflow
ISR_NOERR 5     # Bound Range Exceeded
ISR_NOERR 6     # Invalid Opcode
ISR_NOERR 7     # Device Not Available
ISR_ERR   8     # Double Fault (pushes error code)
ISR_NOERR 9     # Coprocessor Segment Overrun
ISR_ERR   10    # Invalid TSS (pushes error code)
ISR_ERR   11    # Segment Not Present (pushes error code)
ISR_ERR   12    # Stack-Segment Fault (pushes error code)
ISR_ERR   13    # General Protection Fault (pushes error code)
ISR_ERR   14    # Page Fault (pushes error code)
ISR_NOERR 15    # Reserved
ISR_NOERR 16    # x87 Floating-Point Exception
ISR_ERR   17    # Alignment Check (pushes error code)
ISR_NOERR 18    # Machine Check
ISR_NOERR 19    # SIMD Floating-Point Exception
ISR_NOERR 20    # Virtualization Exception
ISR_ERR   21    # Control Protection Exception (pushes error code)
ISR_NOERR 22    # Reserved
ISR_NOERR 23    # Reserved
ISR_NOERR 24    # Reserved
ISR_NOERR 25    # Reserved
ISR_NOERR 26    # Reserved
ISR_NOERR 27    # Reserved
ISR_NOERR 28    # Reserved
ISR_NOERR 29    # Reserved
ISR_NOERR 30    # Reserved
ISR_NOERR 31    # Reserved

# Define IRQ handlers (vectors 32-47)
ISR_NOERR 32    # IRQ0 - Timer
ISR_NOERR 33    # IRQ1 - Keyboard
ISR_NOERR 34    # IRQ2 - Cascade
ISR_NOERR 35    # IRQ3 - COM2
ISR_NOERR 36    # IRQ4 - COM1
ISR_NOERR 37    # IRQ5 - LPT2
ISR_NOERR 38    # IRQ6 - Floppy
ISR_NOERR 39    # IRQ7 - LPT1
ISR_NOERR 40    # IRQ8 - RTC
ISR_NOERR 41    # IRQ9 - Free
ISR_NOERR 42    # IRQ10 - Free
ISR_NOERR 43    # IRQ11 - Free
ISR_NOERR 44    # IRQ12 - PS2 Mouse
ISR_NOERR 45    # IRQ13 - FPU
ISR_NOERR 46    # IRQ14 - Primary ATA
ISR_NOERR 47    # IRQ15 - Secondary ATA

# Common stub that saves state and calls C handler
isr_common_stub:
    # Save all general purpose registers
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # Call C handler with pointer to interrupt frame
    movq %rsp, %rdi           # First argument: pointer to stack frame
    call exception_handler

    # Restore all general purpose registers
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    # Remove error code and interrupt number from stack
    addq $16, %rsp

    # Return from interrupt
    iretq

irq_common_stub:
    # Save all general purpose registers
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # Call IRQ handler (not exception handler!)
    movq %rsp, %rdi
    call irq_handler

    # Restore all general purpose registers
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    # Remove error code and interrupt number
    addq $16, %rsp

    # Return from interrupt
    iretq
