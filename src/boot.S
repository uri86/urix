# boot.S - 64-bit boot file for URIX
# Multiboot2 header for 64-bit kernel
#
# Notes:
#  - This file is written to be loaded by GRUB (Multiboot2).
#  - GRUB places the Multiboot2 info pointer in EBX (on entry).
#  - We store EBX into a global so our 64-bit C entry can read it.
#  - We create minimal page tables that identity-map the low physical
#    memory (enough to run the kernel at 0x0010_0000) using 2 MiB pages.
#  - Page-table entries are 8 bytes; when building them in 32-bit mode
#    we write both low and high dwords to form correct 64-bit entries.
#  - We enable PAE (CR4.PAE) before loading CR3; then set EFER.LME and CR0.PG.

# This must match your linker.ld KERNEL_VIRT_BASE
.set KERNEL_VIRT_BASE, 0xFFFFFFFF80000000

.set MULTIBOOT2_MAGIC, 0xe85250d6
.set GRUB_MULTIBOOT_ARCHITECTURE_I386, 0
.set MULTIBOOT_HEADER_TAG_END, 0
.set MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST, 1

# Multiboot2 header section.
# Must be within the first 32 KiB of the image and 8-byte aligned.
.section .multiboot_header
.align 8
multiboot_header_start:
    .long MULTIBOOT2_MAGIC
    .long GRUB_MULTIBOOT_ARCHITECTURE_I386
    .long multiboot_header_end - multiboot_header_start
    .long -(MULTIBOOT2_MAGIC + GRUB_MULTIBOOT_ARCHITECTURE_I386 + (multiboot_header_end - multiboot_header_start))

    # We request the memory map (type 6) from GRUB/firmware
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
.code32                  # We start in 32-bit mode (GRUB loads us here)
.global _start
.type _start, @function

_start:
    # --- very early CPU setup (we're in 32-bit protected mode) ---
    cli                     # disable interrupts while booting
    
    # Subtract Virtual Base to get physical stack address
    movl $(stack_top - KERNEL_VIRT_BASE), %esp   

    # Clear EFLAGS (nice to have deterministic flags)
    pushl $0
    popfl

    # GRUB passes:
    #   EAX = multiboot2 magic (0x36d76289)
    #   EBX = pointer to the multiboot2 info structure
    #
    # Save EBX into a global so our 64-bit kernel entry can read it later.
    # Use physical address of the pointer variable
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

    # Load the 64-bit GDT (we prepared a 64-bit code segment descriptor)
    # Use physical address for GDT pointer
    lgdt (gdt64_pointer_phys - KERNEL_VIRT_BASE)

    # Jump to physical address of 1: to enter 64-bit mode.
    # We cannot jump to 'long_mode_start' directly because it's at a high virtual address.
    ljmpl $0x08, $(1f - KERNEL_VIRT_BASE)

# -------------------------
# CPU feature helpers
# -------------------------
check_cpuid:
    # Try to flip ID bit in EFLAGS to detect CPUID availability.
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
    # Check if CPU supports extended CPUID leaves and the LM bit.
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb no_long_mode_ret

    movl $0x80000001, %eax
    cpuid
    testl $0x20000000, %edx   # check LM (long mode) bit in EDX
    jz no_long_mode_ret
    movl $1, %eax
    ret

no_long_mode_ret:
    xorl %eax, %eax
    ret

# -------------------------
# Page table setup (32-bit build)
# -------------------------
# We will create three page-sized tables:
#   p4_table (PML4)
#   p3_table (PDPT)
#   p2_table (PD)
#
# We intend to identity map the first 1 GiB of physical memory using:
#   PML4[0] -> PDPT
#   PDPT[0] -> PD
#   PD entries -> 2 MiB pages (512 entries * 2 MiB = 1 GiB)
#
# Important: page-table entries are 8 bytes (64-bit). We are in 32-bit mode,
# so we write each entry as two consecutive 32-bit stores:
#   - low dword: lower 32 bits (base low + flags)
#   - high dword: upper 32 bits (base >> 32), typically 0 for low memory.
#
# This guarantees entries are well formed for the MMU when paging enables.
setup_page_tables:
    # Use physical addresses for all tables (Subtract KERNEL_VIRT_BASE)
    # edi/esi/edx used as temporaries to point at table buffers.
    # Zero each page table (4096 bytes each) using stosl (4-byte stores).

    # Zero PML4 (p4_table)
    movl $(p4_table - KERNEL_VIRT_BASE), %edi
    xorl %eax, %eax
    movl $1024, %ecx
    rep stosl

    # Zero PDPT (p3_table)
    movl $(p3_table - KERNEL_VIRT_BASE), %edi
    xorl %eax, %eax
    movl $1024, %ecx
    rep stosl

    # Zero PD (p2_table)
    movl $(p2_table - KERNEL_VIRT_BASE), %edi
    xorl %eax, %eax
    movl $1024, %ecx
    rep stosl

    # --- Set PML4[0] to point to PDPT (p3_table) --- Identity
    movl $(p3_table - KERNEL_VIRT_BASE), %eax
    orl $0x03, %eax           
    movl $(p4_table - KERNEL_VIRT_BASE), %edi      
    movl %eax, (%edi)         
    movl $0, 4(%edi)          

    # --- [FIX] Set PML4[511] to point to PDPT --- Higher Half
    # Index 511 is the last entry in PML4, covering 0xFFFFFF8000000000+
    # 511 * 8 bytes = 4088
    movl %eax, 4088(%edi)
    movl $0, 4092(%edi)

    # --- Set PDPT[0] to point to PD (p2_table) --- Identity
    movl $(p2_table - KERNEL_VIRT_BASE), %eax
    orl $0x03, %eax           
    movl $(p3_table - KERNEL_VIRT_BASE), %edi      
    movl %eax, (%edi)         
    movl $0, 4(%edi)          

    # --- [FIX] Set PDPT[510] to point to PD --- Higher Half
    # Index 510 covers the -2GB range (0xFFFFFFFF80000000)
    # 510 * 8 bytes = 4080
    movl %eax, 4080(%edi)
    movl $0, 4084(%edi)

    # --- Fill PD entries with 2 MiB pages ---
    # Each PD entry (PDE) will have the physical base and flags:
    #  flags: Present(1) | Writable(2) | PageSize(PS=0x80 for 2MB) => 0x83
    # Start mapping at physical 0x0, then add 2MB for each entry.
        movl $0x83, %eax          
    movl $(p2_table - KERNEL_VIRT_BASE), %edi
    movl $512, %ecx           

fill_p2_table:
    movl %eax, (%edi)         # low 32 bits of PDE (base_low | flags)
    movl $0, 4(%edi)          # high 32 bits of PDE (base_high = 0)
    addl $0x200000, %eax      # increment base low by 2 MiB for next PDE
    addl $8, %edi             # next PDE (8 bytes)
    loop fill_p2_table

    ret

# -------------------------
# Enable paging and long mode
# -------------------------
# Sequence:
# 1) Enable CR4.PAE (so CPU will expect PAE page tables)
# 2) Load CR3 with the physical address of PML4
# 3) Enable LME in EFER MSR (via rdmsr/wrmsr)
# 4) Enable paging by setting CR0.PG
#
# Note: CR3 must contain the physical address of the PML4. Because we used
# absolute symbol addresses for tables in low memory, using movl $p4_table, %eax
# is correct here (we are identity-mapping low memory).
enable_paging:
    # Enable PAE in CR4 first (bit 5).
    movl %cr4, %eax
    orl $0x20, %eax           # set PAE (bit 5)
    movl %eax, %cr4

    # Now load CR3 with the physical address of our PML4 table.
    movl $(p4_table - KERNEL_VIRT_BASE), %eax
    movl %eax, %cr3

    # Enable long mode (LME) by setting bit 8 in EFER MSR.
    movl $0xC0000080, %ecx   # IA32_EFER MSR
    rdmsr                    # EDX:EAX <- MSR
    orl $0x100, %eax         # set LME bit (bit 8)
    wrmsr

    # Finally, enable paging by setting CR0.PG.
    movl %cr0, %eax
    orl $0x80000000, %eax    # set PG bit (bit 31)
    movl %eax, %cr0

    ret

# -------------------------
# 64-bit entry point
# -------------------------
.code64
1:
    # We are now in 64-bit mode (Physical RIP).
    # Perform an absolute jump to the Higher Half.
    movabsq $long_mode_start, %rax
    jmp *%rax

long_mode_start:
    # Now running in Higher Half (0xFFFFFFFF8000xxxx).
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    # Switch to our 64-bit stack (Virtual address)
    movq $stack_top, %rsp

    # Clear screen
    movq $0xb8000, %rdi
    movw $0x0720, %ax          
    movl $2000, %ecx
    rep stosw

    # Load the saved Multiboot2 info pointer and pass it to kernel_main.
    # System V AMD64 calls: first argument in RDI.
    movq multiboot_info_ptr(%rip), %rdi
    call kernel_main

    # If kernel_main returns, just halt here.
halt_loop:
    hlt
    jmp halt_loop

# -------------------------
# Error handlers
# -------------------------
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

# -------------------------
# GDT for 64-bit mode
# -------------------------
.align 16
gdt64:
    .quad 0                    # null descriptor
    .quad 0x00af9a000000ffff    # 64-bit code segment descriptor
    .quad 0x00cf92000000ffff    # data segment descriptor (unused in long mode)
gdt64_end:

# This pointer is used for the INITIAL lgdt while we are still "low"
gdt64_pointer_phys:
    .short gdt64_end - gdt64 - 1
    .long (gdt64 - KERNEL_VIRT_BASE) # Force a 32-bit physical address
    .long 0

# This pointer can be used later by the C kernel
gdt64_pointer:
    .short gdt64_end - gdt64 - 1
    .quad gdt64

# -------------------------
# Data / BSS
# -------------------------
.section .bss
.align 8

# Global place to store the pointer GRUB passed in EBX.
# We use a 64-bit slot (.quad) so 64-bit code can read it directly.
.global multiboot_info_ptr
multiboot_info_ptr:
    .quad 0

# Page tables (each page must be page-aligned)
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
