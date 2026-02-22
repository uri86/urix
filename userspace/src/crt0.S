# Licensed under MIT License - URIX project
# crt0.S - the definition of the _start function for every underlying process in userspace.
# Responsibility - prepare a process to run, and call the main function of the process in userspace (ELF).
.section .text
.global _start

_start:
    # Clear frame pointer
    xor %rbp, %rbp
    
    # rdi=argc, rsi=argv is set by the kernel
    # calling process main
    call main
    
    # exit with main's return value
    mov %rax, %rdi
    mov $0, %rax # SYS_EXIT = 0
    int $0x80 # call interrupt
    
1:  jmp 1b
