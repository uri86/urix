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
    movl $stack_top, %esp   # set up a temporary 32-bit stack

    # Clear EFLAGS (nice to have deterministic flags)
    pushl $0
    popfl

    # GRUB passes:
    #   EAX = multiboot2 magic (0x36d76289)
    #   EBX = pointer to the multiboot2 info structure
    #
    # Save EBX into a global so our 64-bit kernel entry can read it later.
    # We write only the low 32 bits here because the pointer is <4GiB.
    movl %ebx, multiboot_info_ptr

    # Basic CPU feature checks
    call check_cpuid
    test %eax, %eax
    jz no_cpuid

    call check_long_mode
    test %eax, %eax
    jz no_long_mode

    # Build simple page tables that identity-map low memory.
    # Note: setup_page_tables builds the page-table pages and fills entries.
    call setup_page_tables

    # Enable paging and long mode (enable_paging performs the correct
    # control-register/MSR sequence).
    call enable_paging

    # Load the 64-bit GDT (we prepared a 64-bit code segment descriptor)
    lgdt gdt64_pointer

    # Jump to long mode by using a far jump into the 64-bit code segment.
    # This will switch to 64-bit instruction decoding.
    ljmpl $0x08, $long_mode_start

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
    # edi/esi/edx used as temporaries to point at table buffers.
    # Zero each page table (4096 bytes each) using stosl (4-byte stores).

    # Zero PML4 (p4_table)
    movl $p4_table, %edi
    xorl %eax, %eax
    movl $1024, %ecx          # 1024 dwords = 4096 bytes
    rep stosl

    # Zero PDPT (p3_table)
    movl $p3_table, %edi
    xorl %eax, %eax
    movl $1024, %ecx
    rep stosl

    # Zero PD (p2_table)
    movl $p2_table, %edi
    xorl %eax, %eax
    movl $1024, %ecx
    rep stosl

    # --- Set PML4[0] to point to PDPT (p3_table) ---
    # compute low dword: physical address of p3_table OR flags (present|writable)
    movl $p3_table, %eax
    orl $0x03, %eax           # Present (bit0) + Writable (bit1)
    movl $p4_table, %edi      # place pointer at start of p4_table
    movl %eax, (%edi)         # low 32 bits: base + flags
    movl $0, 4(%edi)          # high 32 bits: zero (p3_table < 4 GiB)

    # --- Set PDPT[0] to point to PD (p2_table) ---
    movl $p2_table, %eax
    orl $0x03, %eax           # Present + Writable
    movl $p3_table, %edi      # PDPT base
    movl %eax, (%edi)         # PDPT[0] low dword
    movl $0, 4(%edi)          # PDPT[0] high dword = 0

    # --- Fill PD entries with 2 MiB pages ---
    # Each PD entry (PDE) will have the physical base and flags:
    #  flags: Present(1) | Writable(2) | PageSize(PS=0x80 for 2MB) => 0x83
    # Start mapping at physical 0x0, then add 2MB for each entry.
    movl $0x83, %eax          # initial low dword: flags + low base (0)
    movl $p2_table, %edi
    movl $512, %ecx           # 512 entries * 2 MiB = 1 GiB mapped

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
    movl $p4_table, %eax
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
long_mode_start:
    # Now running 64-bit instruction decoding (we used a far jump).
    # Registers are still usable; set up a clean 64-bit stack and segments.

    # Set up flat data segments (the selectors map to our GDT entries)
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    # Switch to our 64-bit stack
    movq $stack_top, %rsp

    # (Optional) clear the screen via VGA text buffer to show we are alive.
    movq $0xb8000, %rdi
    movw $0x0720, %ax          # space with light gray on black
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
# Error handlers (print a message on VGA then halt)
# -------------------------
no_cpuid:
no_long_mode:
    movl $0xb8000, %edi
    movl $error_msg, %esi
    movb $0x4f, %ah            # red background, white text
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
