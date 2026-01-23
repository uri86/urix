# process_switch.S - Low-level context switching for URIX
# Licensed under MIT License - URIX project.
#
#  - Support for NULL old_context (first run or exiting process)
#  - User space (ring 3) support
#  - Correct stack frame handling
#  - Proper segment register restoration
#
# This works with the improved process.c scheduler

.section .text
.code64

#
# process_context_switch - Switch between process contexts
#
# Arguments:
#   RDI = Pointer to old process cpu_context_t (can be NULL!)
#   RSI = Pointer to new process cpu_context_t (must not be NULL)
#
# When old_context is NULL (first run or process exiting):
#   - Skip saving current state
#   - Just restore new context and jump
#
# When old_context is valid (normal switch):
#   - Save current CPU state to old_context
#   - Restore CPU state from new_context
#
# cpu_context_t structure offsets (must match process.h):
#   0x00: r15       0x40: rbp      0x78: rip
#   0x08: r14       0x48: rdi      0x80: cs
#   0x10: r13       0x50: rsi      0x88: rflags
#   0x18: r12       0x58: rdx      0x90: rsp
#   0x20: r11       0x60: rcx      0x98: ss
#   0x28: r10       0x68: rbx
#   0x30: r9        0x70: rax
#   0x38: r8

.global process_context_switch
.type process_context_switch, @function
process_context_switch:
    # Check if old_context is NULL
    testq %rdi, %rdi
    jz restore_new_context    # If NULL, skip saving
    
    
    # Save general-purpose registers
    movq %r15, 0x00(%rdi)
    movq %r14, 0x08(%rdi)
    movq %r13, 0x10(%rdi)
    movq %r12, 0x18(%rdi)
    movq %r11, 0x20(%rdi)
    movq %r10, 0x28(%rdi)
    movq %r9,  0x30(%rdi)
    movq %r8,  0x38(%rdi)
    movq %rbp, 0x40(%rdi)
    
    # Save RDI and RSI
    movq %rdi, 0x48(%rdi)
    movq %rsi, 0x50(%rdi)
    
    movq %rdx, 0x58(%rdi)
    movq %rcx, 0x60(%rdi)
    movq %rbx, 0x68(%rdi)
    movq %rax, 0x70(%rdi)
    
    # Save instruction pointer (return address on stack)
    movq (%rsp), %rax         # Get return address
    movq %rax, 0x78(%rdi)     # Save as RIP
    
    # Save code segment
    movq $0x08, %rax          # Kernel CS
    movq %rax, 0x80(%rdi)
    
    # Save RFLAGS
    pushfq
    popq %rax
    movq %rax, 0x88(%rdi)
    
    # Save stack pointer (before call, so RSP + 8 to skip return address)
    leaq 8(%rsp), %rax        # RSP before call instruction
    movq %rax, 0x90(%rdi)
    
    # Save stack segment
    movq $0x10, %rax          # Kernel DS
    movq %rax, 0x98(%rdi)
    
restore_new_context:
    # RSI points to new_context
    
    # We need to check if this is a kernel or user mode process
    # by looking at the CS register in the context
    movq 0x80(%rsi), %rax     # Get CS from new context
    andq $3, %rax             # Check RPL (last 2 bits)
    cmpq $3, %rax             # Is it ring 3?
    je restore_user_mode      # Yes, use iretq path
    
restore_kernel_mode:
    # Simple restore for ring 0 processes
    
    # Check if this is the very first context switch (from kernel_main)
    # by checking if we have a return address on stack
    # If old_context was NULL, we jumped here with no return address
    cmpq $0, %rdi                 # Was old_context NULL?
    jne normal_kernel_restore     # No, do normal restore with ret
    
first_process_jump:
    # First process
    # Just restore everything and jump to RIP directly
    
    # Restore stack pointer
    movq 0x90(%rsi), %rsp
    
    # Restore general-purpose registers
    movq 0x00(%rsi), %r15
    movq 0x08(%rsi), %r14
    movq 0x10(%rsi), %r13
    movq 0x18(%rsi), %r12
    movq 0x20(%rsi), %r11
    movq 0x28(%rsi), %r10
    movq 0x30(%rsi), %r9
    movq 0x38(%rsi), %r8
    movq 0x40(%rsi), %rbp
    movq 0x58(%rsi), %rdx
    movq 0x60(%rsi), %rcx
    movq 0x68(%rsi), %rbx
    movq 0x70(%rsi), %rax
    movq 0x48(%rsi), %rdi
    
    # Restore RFLAGS
    pushq 0x88(%rsi)
    popfq
    
    # Get RIP into a register
    movq 0x78(%rsi), %r11
    
    # Restore RSI last
    movq 0x50(%rsi), %rsi
    
    # Jump to entry point (not return)
    jmp *%r11
    
normal_kernel_restore:
    
    # Restore stack pointer first
    movq 0x90(%rsi), %rsp
    
    # Restore general-purpose registers
    movq 0x00(%rsi), %r15
    movq 0x08(%rsi), %r14
    movq 0x10(%rsi), %r13
    movq 0x18(%rsi), %r12
    movq 0x20(%rsi), %r11
    movq 0x28(%rsi), %r10
    movq 0x30(%rsi), %r9
    movq 0x38(%rsi), %r8
    movq 0x40(%rsi), %rbp
    movq 0x58(%rsi), %rdx
    movq 0x60(%rsi), %rcx
    movq 0x68(%rsi), %rbx
    movq 0x70(%rsi), %rax
    
    # Restore RFLAGS
    pushq 0x88(%rsi)
    popfq
    
    # Restore RDI
    movq 0x48(%rsi), %rdi
    
    # Push return address (RIP) onto stack
    pushq 0x78(%rsi)
    
    # Restore RSI last
    movq 0x50(%rsi), %rsi
    
    # Return to new process (pops RIP and jumps)
    ret
    
restore_user_mode:
    # Must use iretq to switch privilege levels
    # iretq expects stack frame: SS, RSP, RFLAGS, CS, RIP
    
    # Build iretq frame on current (kernel) stack
    # Switch to a temporary kernel stack to build frame
    
    # Get new kernel RSP
    movq 0x90(%rsi), %rsp
    
    # Build iretq stack frame (pushed in reverse order)
    pushq 0x98(%rsi)          # SS (user data segment)
    pushq 0x90(%rsi)          # RSP (user stack pointer)
    pushq 0x88(%rsi)          # RFLAGS
    pushq 0x80(%rsi)          # CS (user code segment)
    pushq 0x78(%rsi)          # RIP (entry point)
    
    # Restore general-purpose registers
    movq 0x00(%rsi), %r15
    movq 0x08(%rsi), %r14
    movq 0x10(%rsi), %r13
    movq 0x18(%rsi), %r12
    movq 0x20(%rsi), %r11
    movq 0x28(%rsi), %r10
    movq 0x30(%rsi), %r9
    movq 0x38(%rsi), %r8
    movq 0x40(%rsi), %rbp
    movq 0x58(%rsi), %rdx
    movq 0x60(%rsi), %rcx
    movq 0x68(%rsi), %rbx
    movq 0x70(%rsi), %rax
    movq 0x48(%rsi), %rdi
    
    # Restore RSI last
    movq 0x50(%rsi), %rsi
    
    # Execute iretq:
    #  1. Pop RIP
    #  2. Pop CS (and switch to ring 3 if RPL=3)
    #  3. Pop RFLAGS
    #  4. Pop RSP
    #  5. Pop SS
    #  6. Resume execution in new privilege level
    iretq
