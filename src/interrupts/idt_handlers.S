# idt_handlers.S - Low-level interrupt handler stubs for URIX
# Licensed under MIT License - URIX project.
#
# Responsibilities:
#  - save CPU state on interrupt entry
#  - push interrupt number and error code
#  - call high-level C exception handler
#  - restore CPU state on return
#

.section .text
.code64

# Macro for ISRs that don't push error codes
.macro ISR_NOERR num
.global isr\num
.type isr\num, @function
isr\num:
    pushq $0 # Push dummy error code
    pushq $\num # Push interrupt number
    jmp isr_common_stub
.endm

# Macro for ISRs that push error codes
.macro ISR_ERR num
.global isr\num
.type isr\num, @function
isr\num:
    pushq $\num # Push interrupt number
    jmp isr_common_stub
.endm

# Macro for IRQ handlers
.macro IRQ_HANDLER num
.global isr\num
.type isr\num, @function
isr\num:
    pushq $0 # Push dummy error code
    pushq $\num # Push interrupt number
    jmp irq_common_stub
.endm

# Define exception handlers
ISR_NOERR 0     # Division By Zero
ISR_NOERR 1     # Debug
ISR_NOERR 2     # Non Maskable Interrupt
ISR_NOERR 3     # Breakpoint
ISR_NOERR 4     # Overflow
ISR_NOERR 5     # Bound Range Exceeded
ISR_NOERR 6     # Invalid Opcode
ISR_NOERR 7     # Device Not Available
ISR_ERR   8     # Double Fault
ISR_NOERR 9     # Coprocessor Segment Overrun
ISR_ERR   10    # Invalid TSS
ISR_ERR   11    # Segment Not Present
ISR_ERR   12    # Stack-Segment Fault
ISR_ERR   13    # General Protection Fault
ISR_ERR   14    # Page Fault
ISR_NOERR 15
ISR_NOERR 16    # x87 Floating-Point Exception
ISR_ERR   17    # Alignment Check
ISR_NOERR 18    # Machine Check
ISR_NOERR 19    # SIMD Floating-Point Exception
ISR_NOERR 20    # Virtualization Exception
ISR_ERR   21    # Control Protection Exception
# 22 to 31 are reserved
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

# Define IRQ handlers
IRQ_HANDLER 32    # IRQ0 - Timer
IRQ_HANDLER 33    # IRQ1 - Keyboard
IRQ_HANDLER 34    # IRQ2 - Cascade
IRQ_HANDLER 35    # IRQ3 - COM2
IRQ_HANDLER 36    # IRQ4 - COM1
IRQ_HANDLER 37    # IRQ5 - LPT2
IRQ_HANDLER 38    # IRQ6 - Floppy
IRQ_HANDLER 39    # IRQ7 - LPT1
IRQ_HANDLER 40    # IRQ8 - RTC
# 9 - 11 Free for use
IRQ_HANDLER 41
IRQ_HANDLER 42
IRQ_HANDLER 43
IRQ_HANDLER 44    # IRQ12 - PS2 Mouse
IRQ_HANDLER 45    # IRQ13 - FPU
IRQ_HANDLER 46    # IRQ14 - Primary ATA
IRQ_HANDLER 47    # IRQ15 - Secondary ATA

# Common stub for exceptions, saves state and calls C handler
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

    movq %rsp, %rdi # pointer to stack frame
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

# Common stub for IRQs, saves state and calls IRQ handler
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

    # Call IRQ handler
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