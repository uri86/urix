# process_switch.S - Low-level context switching for URIX
# Licensed under MIT License - URIX project.

.section .text
.code64

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
    
    # Save instruction pointer
    movq (%rsp), %rax         # Get return address
    movq %rax, 0x78(%rdi)     # Save as RIP
    
    # Save code segment
    movq $0x08, %rax
    movq %rax, 0x80(%rdi)
    
    # Save RFLAGS
    pushfq
    popq %rax
    movq %rax, 0x88(%rdi)
    
    # Save stack pointer
    leaq 8(%rsp), %rax
    movq %rax, 0x90(%rdi)
    
    # Save stack segment
    movq $0x10, %rax
    movq %rax, 0x98(%rdi)
    
restore_new_context:
    movq 0x80(%rsi), %rax     # Get CS from new context
    andq $3, %rax
    cmpq $3, %rax
    je restore_user_mode
    
restore_kernel_mode:
    cmpq $0, %rdi
    jne normal_kernel_restore
    
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
    
    # Jump to entry point
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
    
    # Return to new process
    ret
    
restore_user_mode:
    movq 0x90(%rsi), %rsp
    
    # Build iretq stack frame
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
    iretq
