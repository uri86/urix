# boot.S - 64-bit boot file for URIX
# Multiboot2 header for 64-bit kernel

.set MULTIBOOT2_MAGIC, 0xe85250d6
.set GRUB_MULTIBOOT_ARCHITECTURE_I386, 0
.set MULTIBOOT_HEADER_TAG_END, 0
.set MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST, 1

.section .multiboot_header
.align 8
multiboot_header_start:
    .long MULTIBOOT2_MAGIC
    .long GRUB_MULTIBOOT_ARCHITECTURE_I386
    .long multiboot_header_end - multiboot_header_start
    .long -(MULTIBOOT2_MAGIC + GRUB_MULTIBOOT_ARCHITECTURE_I386 + (multiboot_header_end - multiboot_header_start))

    # Information request tag
    .align 8
    .short MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST
    .short 0
    .long 12
    .long 6  # Memory map

    # End tag
    .align 8
    .short MULTIBOOT_HEADER_TAG_END
    .short 0
    .long 8
multiboot_header_end:

.section .text
.code32  # We start in 32-bit mode
.global _start
.type _start, @function

_start:
    # We're still in 32-bit mode when GRUB loads us
    cli
    
    # Set up stack
    movl $stack_top, %esp
    
    # Reset EFLAGS
    pushl $0
    popfl
    
    # Save multiboot info
    pushl %eax  # Magic number
    pushl %ebx  # Multiboot info pointer
    
    # Check if CPUID is supported
    call check_cpuid
    test %eax, %eax
    jz no_cpuid
    
    # Check if long mode is supported
    call check_long_mode
    test %eax, %eax
    jz no_long_mode
    
    # Set up paging for long mode
    call setup_page_tables
    call enable_paging
    
    # Load 64-bit GDT
    lgdt gdt64_pointer
    
    # Jump to 64-bit code
    ljmpl $0x08, $long_mode_start

check_cpuid:
    # Try to flip ID bit in EFLAGS
    pushfl
    popl %eax
    movl %eax, %ecx
    xorl $0x200000, %eax
    pushl %eax
    popfl
    pushfl
    popl %eax
    pushl %ecx
    popfl
    xorl %ecx, %eax
    ret

check_long_mode:
    # Check if extended processor info is available
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb no_long_mode_ret
    
    # Check if long mode is available
    movl $0x80000001, %eax
    cpuid
    testl $0x20000000, %edx  # LM bit
    jz no_long_mode_ret
    movl $1, %eax
    ret

no_long_mode_ret:
    xorl %eax, %eax
    ret

setup_page_tables:
    # Map first 2MB with huge pages
    # PML4 -> PDPT -> PD -> 2MB pages
    
    # Clear page tables
    movl $p4_table, %edi
    movl $p3_table, %esi
    movl $p2_table, %edx
    
    # Zero out the page tables
    xorl %eax, %eax
    movl $1024, %ecx
    rep stosl
    
    movl $1024, %ecx
    movl %esi, %edi
    rep stosl
    
    movl $1024, %ecx
    movl %edx, %edi
    rep stosl
    
    # Set up PML4 entry
    movl $p3_table, %eax
    orl $0x03, %eax  # Present + writable
    movl %eax, p4_table
    
    # Set up PDPT entry
    movl $p2_table, %eax
    orl $0x03, %eax  # Present + writable
    movl %eax, p3_table
    
    # Set up PD entries (2MB pages)
    movl $0x83, %eax  # Present + writable + huge page
    movl $p2_table, %edi
    movl $512, %ecx
    
fill_p2_table:
    movl %eax, (%edi)
    addl $0x200000, %eax  # 2MB
    addl $8, %edi
    loop fill_p2_table
    
    ret

enable_paging:
    # Load P4 table
    movl $p4_table, %eax
    movl %eax, %cr3
    
    # Enable PAE
    movl %cr4, %eax
    orl $0x20, %eax
    movl %eax, %cr4
    
    # Enable long mode
    movl $0xC0000080, %ecx  # EFER MSR
    rdmsr
    orl $0x100, %eax  # LME bit
    wrmsr
    
    # Enable paging
    movl %cr0, %eax
    orl $0x80000000, %eax
    movl %eax, %cr0
    
    ret

.code64  # Switch to 64-bit assembly syntax
long_mode_start:
    # We're now in 64-bit mode!
    
    # Set up segment registers
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    
    # Set up 64-bit stack
    movq $stack_top, %rsp
    
    # Clear the screen (optional)
    movq $0xb8000, %rdi
    movw $0x0720, %ax  # Space with light gray on black
    movl $2000, %ecx
    rep stosw
    
    # Jump to kernel
    call kernel_main
    
    # Halt if kernel returns
halt_loop:
    hlt
    jmp halt_loop

no_cpuid:
no_long_mode:
    # Print error message and halt
    movl $0xb8000, %edi
    movl $error_msg, %esi
    movb $0x4f, %ah  # Red background, white text
print_error:
    lodsb
    testb %al, %al
    jz halt_loop
    stosw
    jmp print_error

.section .rodata
error_msg: .asciz "64-bit not supported!"

# GDT for 64-bit mode
.align 16
gdt64:
    .quad 0                 # Null segment
    .quad 0x00af9a000000ffff # Code segment (64-bit)
    .quad 0x00cf92000000ffff # Data segment
gdt64_end:

gdt64_pointer:
    .short gdt64_end - gdt64 - 1
    .quad gdt64

# Page tables (must be page-aligned)
.section .bss
.align 4096
p4_table:
    .skip 4096
p3_table:
    .skip 4096
p2_table:
    .skip 4096

# Stack
.align 16
stack_bottom:
    .skip 65536  # 64KB stack
stack_top: