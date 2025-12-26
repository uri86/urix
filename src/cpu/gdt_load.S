# gdt_load.S - Assembly routines for loading GDT and TSS
# Licensed under MIT License - URIX project.
#
# Responsibilities:
#  - Load new GDT into CPU
#  - Reload segment registers with new selectors
#  - Load TSS register
#
# Notes:
#  - LGDT instruction loads GDT register
#  - After loading GDT, must reload all segment registers
#  - CS (code segment) requires far return to reload
#  - TSS loaded with LTR (Load Task Register) instruction

.section .text
.code64

#
# gdt_load - Load the Global Descriptor Table
#
# Arguments:
#   RDI = Address of GDT pointer structure (limit + base)
#
# This function:
#  1. Loads the new GDT using LGDT instruction
#  2. Reloads all segment registers with new values
#  3. Does a far return to reload CS
#
.global gdt_load
.type gdt_load, @function
gdt_load:
    # Load the new GDT
    # LGDT takes a pointer to a 10-byte structure:
    #   2 bytes: limit (size of GDT - 1)
    #   8 bytes: base address of GDT
    lgdt (%rdi)

    # Reload data segment registers
    # All data segments (DS, ES, FS, GS, SS) need to be reloaded
    # with the new kernel data segment selector (0x10 = GDT_KERNEL_DATA)
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    # Reload code segment (CS)
    # CS cannot be loaded directly with MOV
    # We must use a far return which pops CS:RIP from stack
    # 
    # Push new CS selector (0x08 = GDT_KERNEL_CODE)
    # Push return address (.reload_cs label)
    # Execute far return (RETFQ = return far 64-bit)
    popq %rax               # Save return address
    pushq $0x08             # Push new CS selector
    pushq %rax              # Push return address back
    lretq                   # Far return: pops RIP then CS

#
# tss_load - Load the Task State Segment
#
# Arguments:
#   DI = TSS selector (16-bit segment selector)
#
#
.global tss_load
.type tss_load, @function
tss_load:
    # Load TSS selector into task register
    # LTR takes a 16-bit selector as operand
    # The selector points to the TSS descriptor in the GDT
    ltr %di
    ret