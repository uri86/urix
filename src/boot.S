# boot.S - 64-bit boot file for URIX
# Multiboot2 header for 64-bit kernel

.set KERNEL_VIRT_BASE, 0xFFFFFFFF80000000

.set MULTIBOOT2_MAGIC, 0xe85250d6
.set GRUB_MULTIBOOT_ARCHITECTURE_I386, 0
.set MULTIBOOT_HEADER_TAG_END, 0
.set MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST, 1

# Multiboot2 header section
.section .multiboot_header
.align 8
multiboot_header_start:
    .long MULTIBOOT2_MAGIC
    .long GRUB_MULTIBOOT_ARCHITECTURE_I386
    .long multiboot_header_end - multiboot_header_start
    .long -(MULTIBOOT2_MAGIC + GRUB_MULTIBOOT_ARCHITECTURE_I386 + (multiboot_header_end - multiboot_header_start))

    # We request the memory map from GRUB
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
.code32
.global _start
.type _start, @function

_start:
    cli # disable interrupts while booting
    
    # Subtract Virtual Base to get physical stack address
    movl $(stack_top - KERNEL_VIRT_BASE), %esp   

    # Clear EFLAGS
    pushl $0
    popfl

    movl %ebx, (multiboot_info_ptr - KERNEL_VIRT_BASE)

    # Basic CPU feature checks
    call check_cpuid
    test %eax, %eax
    jz no_cpuid

    call check_long_mode
    test %eax, %eax
    jz no_long_mode

    # Build simple page tables that identity-map low memory.
    call setup_page_tables

    # Enable paging and long mode.
    call enable_paging

    # Load the 64-bit GDT
    # Use physical address for GDT pointer
    lgdt (gdt64_pointer_phys - KERNEL_VIRT_BASE)
    ljmpl $0x08, $(1f - KERNEL_VIRT_BASE)

check_cpuid:
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
    # Check if CPU supports extended CPUID and the LM bit.
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb no_long_mode_ret

    movl $0x80000001, %eax
    cpuid
    testl $0x20000000, %edx   # check LM bit in EDX
    jz no_long_mode_ret
    movl $1, %eax
    ret

no_long_mode_ret:
    xorl %eax, %eax
    ret


setup_page_tables:
    # Zero all 3 page tables (p4, p3, p2) at once since they are contiguous in .bss
    # 4096 bytes * 3 = 12288 bytes = 3072 dwords
    movl $(p4_table - KERNEL_VIRT_BASE), %edi
    xorl %eax, %eax
    movl $3072, %ecx
    rep stosl

    # Set PML4[0] to point to PDPT, p3_table
    movl $(p3_table - KERNEL_VIRT_BASE), %eax
    orl $0x03, %eax           
    movl $(p4_table - KERNEL_VIRT_BASE), %edi      
    movl %eax, (%edi)         
    movl $0, 4(%edi)          

    movl %eax, 4088(%edi)
    movl $0, 4092(%edi)

    # Set PDPT[0] to point to PD, p2_table
    movl $(p2_table - KERNEL_VIRT_BASE), %eax
    orl $0x03, %eax           
    movl $(p3_table - KERNEL_VIRT_BASE), %edi      
    movl %eax, (%edi)         
    movl $0, 4(%edi)          

    movl %eax, 4080(%edi)
    movl $0, 4084(%edi)

    movl $0x83, %eax          
    movl $(p2_table - KERNEL_VIRT_BASE), %edi
    movl $512, %ecx           

fill_p2_table:
    movl %eax, (%edi)
    movl $0, 4(%edi)
    addl $0x200000, %eax
    addl $8, %edi
    loop fill_p2_table

    ret

enable_paging:
    movl %cr4, %eax
    orl $0x20, %eax           # set PAE (bit 5)
    movl %eax, %cr4

    movl $(p4_table - KERNEL_VIRT_BASE), %eax
    movl %eax, %cr3

    movl $0xC0000080, %ecx
    rdmsr
    orl $0x100, %eax
    wrmsr

    # Finally, enable paging by setting CR0.PG.
    movl %cr0, %eax
    orl $0x80000000, %eax    # set PG bit (bit 31)
    movl %eax, %cr0

    ret

.code64
1:
    movabsq $long_mode_start, %rax
    jmp *%rax

long_mode_start:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    movq $stack_top, %rsp

    # Clear screen
    movq $0xb8000, %rdi
    movw $0x0720, %ax          
    movl $2000, %ecx
    rep stosw

    # Load the saved Multiboot2 info pointer and pass it to kernel_main.
    movq multiboot_info_ptr(%rip), %rdi
    call kernel_main

    # If kernel_main returns, just halt here.
halt_loop:
    hlt
    jmp halt_loop

no_cpuid:
no_long_mode:
    movl $0xb8000, %edi
    movl $(error_msg - KERNEL_VIRT_BASE), %esi
    movb $0x4f, %ah            
print_error:
    lodsb
    testb %al, %al
    jz halt_loop
    stosw
    jmp print_error

.section .rodata
error_msg: .asciz "64-bit not supported!"

.align 16
gdt64:
    .quad 0
    .quad 0x00af9a000000ffff
    .quad 0x00cf92000000ffff
gdt64_end:

gdt64_pointer_phys:
    .short gdt64_end - gdt64 - 1
    .long (gdt64 - KERNEL_VIRT_BASE)
    .long 0

gdt64_pointer:
    .short gdt64_end - gdt64 - 1
    .quad gdt64

.section .bss
.align 8

.global multiboot_info_ptr
multiboot_info_ptr:
    .quad 0

# Page tables
.align 4096
p4_table:
    .skip 4096
p3_table:
    .skip 4096
p2_table:
    .skip 4096

# Stack (64 KiB aligned)
.align 16
stack_bottom:
    .skip 65536   # 64 KiB stack
stack_top:


# Dictionary:
# PDPT — Page Directory Pointer Table (PML3). Third-level page-table; entries (PDPEs) point to PDs.
# PDE — Page Directory Entry. 64-bit entry in a PD that either points to a PT or — with the PS bit set — maps a large page (2 MiB or 1 GiB).
# PD — Page Directory (PML2). Contains PDEs; in this file each PDE maps a 2 MiB page.
# PTE — Page Table Entry. 64-bit entry in a Page Table (PML1) that maps a 4 KiB page (not used here).
# PDPE — Page Directory Pointer Entry. Entry in the PDPT that points to a PD.
# PT — Page Table (PML1). Lowest-level table containing PTEs for 4 KiB pages.
# PML4 — Page Map Level 4 (top-level table). The highest-level page-table in the 4-level x86-64 hierarchy.
# GDT — Global Descriptor Table. Defines segment descriptors (code/data) used when switching modes.
# MSR — Model Specific Register. CPU registers accessed via RDMSR/WRMSR (e.g., IA32_EFER).
# CR0 / CR3 / CR4 — Control registers. CR0 controls paging (PG), CR3 holds the PML4 physical base, CR4 enables features like PAE.
# EFER — Extended Feature Enable Register (MSR). Contains the LME (Long Mode Enable) bit used to enable 64-bit mode.
# LM — Long Mode. The x86-64 64-bit execution mode (requires EFER.LME set and paging enabled).
# PAE — Physical Address Extension. A CR4 feature that changes page-table expectations to support 64-bit entries; required before enabling long mode.
# PS — Page Size bit. When set in a PDE it selects a large page (here used for 2 MiB pages).
# VGA — Video text buffer (physical 0xB8000). Used here to print early boot/status/error messages.
# GRUB — GRand Unified Bootloader. Loads Multiboot2-compliant images and provides the multiboot info pointer.
# Multiboot2 — Multiboot version 2 specification for headers and the boot information structure (memory map, modules, tags).
# Multiboot2 info pointer — Pointer provided by GRUB (in EBX on entry) to the Multiboot2 information structure.
# PDE flags — Typical low-bit flags in page-table entries: Present (1), Writable (2), User (4), PS (0x80), etc. This file uses 0x83 (Present | Writable | PS).
