# Licensed under MIT License - URIX project
# syscall_entry.S - the entry function for syscalls, written in assembly
.section .bss
.global syscall_saved_user_rip
.global syscall_saved_user_rsp
.global syscall_saved_user_rflags
.global syscall_saved_user_cs
.global syscall_saved_user_ss
.global syscall_frame_rsp

syscall_saved_user_rip:    .quad 0
syscall_saved_user_rsp:    .quad 0
syscall_saved_user_rflags: .quad 0
syscall_saved_user_cs:     .quad 0
syscall_saved_user_ss:     .quad 0
syscall_frame_rsp:         .quad 0

.section .text
.code64

.global syscall_entry
.type syscall_entry, @function
syscall_entry:
    # Reload kernel data segments — on entry from ring 3 these are null
    pushq %rax
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    popq %rax

    # Save all general purpose registers
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax     # syscall number

    # Save user-mode iretq frame values for sys_fork
    movq 120(%rsp), %rax
    movq %rax, syscall_saved_user_rip(%rip)
    movq 128(%rsp), %rax
    movq %rax, syscall_saved_user_cs(%rip)
    movq 136(%rsp), %rax
    movq %rax, syscall_saved_user_rflags(%rip)
    movq 144(%rsp), %rax
    movq %rax, syscall_saved_user_rsp(%rip)
    movq 152(%rsp), %rax
    movq %rax, syscall_saved_user_ss(%rip)

    # Save pointer to iretq frame for sys_exec to rewrite
    leaq 120(%rsp), %rax
    movq %rax, syscall_frame_rsp(%rip)

    sti

    # Load arguments into correct registers for syscall_handler
    movq 0(%rsp),  %rdi    # arg0: syscall number
    movq 40(%rsp), %rsi    # arg1: rdi (first user arg)
    movq 32(%rsp), %rdx    # arg2: rsi
    movq 24(%rsp), %rcx    # arg3: rdx
    movq 72(%rsp), %r8     # arg4: r10
    movq 56(%rsp), %r9     # arg5: r9

    # arg6 as stack argument
    pushq 64(%rsp)
    pushq $0               # stack alignment

    call syscall_handler

    # Clean up stack args
    addq $16, %rsp

    # Store return value back into saved rax slot
    movq %rax, 0(%rsp)

    # Restore all general purpose registers
    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rbp
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    # Restore user data segments before returning to ring 3
    pushq %rax
    movw $0x00, %ax
    movw %ax, %ds
    movw %ax, %es
    popq %rax

    iretq