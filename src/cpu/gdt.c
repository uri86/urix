/*
 * Licensed under MIT License - URIX project.
 * gdt.c - Global Descriptor Table implementation.
 */

#include <cpu/gdt.h>
#include <lib/print.h>
#include <string.h>
#include <memory/vmm.h>

/*
 * GDT Table
 *
 * 6 entries:
 *  0. Null descriptor (required by x86)
 *  1. Kernel code segment
 *  2. Kernel data segment
 *  3. User code segment
 *  4. User data segment
 *  5-6. TSS (takes 2 entries in 64-bit mode)
 */
static gdt_entry_t gdt[7];
static gdt_ptr_t gdt_ptr;
static tss_t tss;

/*
 * External assembly function to load the GDT
 * Defined in gdt_load.S
 */
extern void gdt_load(uint64_t gdt_ptr_addr);

/*
 * External assembly function to load the TSS
 * Defined in gdt_load.S
 */
extern void tss_load(uint16_t tss_selector);

/**
 * gdt_set_gate - Set a GDT entry
 *
 * num: GDT entry index (0-6)
 * base: Base address (ignored in 64-bit mode for code/data)
 * limit: Segment limit (ignored in 64-bit mode for code/data)
 * access: Access byte (privilege, type, etc.)
 * gran: Granularity byte (flags + upper limit bits)
 */
static void gdt_set_gate(int num, uint64_t base, uint32_t limit,
                         uint8_t access, uint8_t gran)
{
    /* Set base address */
    gdt[num].base_low = base & 0xFFFF;
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    /* Set limit */
    gdt[num].limit_low = limit & 0xFFFF;
    gdt[num].granularity = (limit >> 16) & 0x0F;

    /* Set granularity and access */
    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access = access;
}

/**
 * gdt_set_tss - Set the TSS entry in GDT
 *
 * num: GDT entry index where TSS starts
 * tss_addr: Address of TSS structure
 */
static void gdt_set_tss(int num, uint64_t tss_addr)
{
    uint64_t base = tss_addr;
    uint32_t limit = sizeof(tss_t) - 1;

    /* First entry (lower 8 bytes) */
    gdt[num].limit_low = limit & 0xFFFF;
    gdt[num].base_low = base & 0xFFFF;
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].access = GDT_TSS_ACCESS;
    gdt[num].granularity = ((limit >> 16) & 0x0F) | GDT_GRAN_BYTE;
    gdt[num].base_high = (base >> 24) & 0xFF;

    /* Second entry (upper 8 bytes) - cast to tss_entry_t for convenience */
    tss_entry_t *tss_entry = (tss_entry_t *)&gdt[num];
    tss_entry->base_upper = (base >> 32) & 0xFFFFFFFF;
    tss_entry->reserved = 0;
}

void gdt_init(void)
{
    kprintf("\n=== Initializing GDT ===\n");

    /* Clear GDT and TSS */
    memset(&gdt, 0, sizeof(gdt));
    memset(&tss, 0, sizeof(tss));

    gdt_set_gate(0, 0, 0, 0, 0);
    debug_kprintf("Entry 0: Null descriptor\n");

    /*
     * Kernel Code Segment (64-bit, ring 0)
     *
     * Access: Present | Ring0 | Code/Data | Executable | Readable
     * Granularity: 64-bit long mode
     *
     * This is the segment kernel code runs in.
     */
    gdt_set_gate(1, 0, 0xFFFFFFFF,
                 GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_CODESEG |
                     GDT_ACCESS_EXECUTABLE | GDT_ACCESS_READWRITE,
                 GDT_GRAN_64BIT | GDT_GRAN_4K);
    debug_kprintf("Entry 1: Kernel Code (Ring 0, 64-bit)\n");

    /*
     * Kernel Data Segment (ring 0)
     *
     * Access: Present | Ring0 | Code/Data | Writable
     * Granularity: 32-bit
     *
     * Used for data access in kernel mode.
     */
    gdt_set_gate(2, 0, 0xFFFFFFFF,
                 GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_CODESEG |
                     GDT_ACCESS_READWRITE,
                 GDT_GRAN_32BIT | GDT_GRAN_4K);
    debug_kprintf("Entry 2: Kernel Data (Ring 0)\n");

    /*
     * User Code Segment (64-bit, ring 3)
     *
     * Same as kernel code but with Ring 3 privilege.
     */
    gdt_set_gate(3, 0, 0xFFFFFFFF,
                 GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_CODESEG |
                     GDT_ACCESS_EXECUTABLE | GDT_ACCESS_READWRITE,
                 GDT_GRAN_64BIT | GDT_GRAN_4K);
    debug_kprintf("Entry 3: User Code (Ring 3, 64-bit)\n");

    /*
     * User Data Segment (ring 3)
     *
     * Used for data access in user mode.
     */
    gdt_set_gate(4, 0, 0xFFFFFFFF,
                 GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_CODESEG |
                     GDT_ACCESS_READWRITE,
                 GDT_GRAN_32BIT | GDT_GRAN_4K);
    debug_kprintf("Entry 4: User Data (Ring 3)\n");

    /*
     * TSS (Task State Segment)
     */
    gdt_set_tss(5, (uint64_t)&tss);
    kprintf("Entry 5-6: TSS at %llx\n", (uint64_t)&tss);

    /* Initialize TSS */
    tss.rsp0 = 0;
    tss.iopb_offset = sizeof(tss_t);

    /* Set up GDT pointer */
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base = (uint64_t)&gdt;

    /* Load the GDT */
    kprintf("Loading GDT at %llx (limit: %u)...\n", gdt_ptr.base, gdt_ptr.limit);
    gdt_load((uint64_t)&gdt_ptr);

    /* Load the TSS */
    kprintf("Loading TSS (selector: 0x%x)...\n", GDT_TSS);
    tss_load(GDT_TSS);

    kprintf("GDT initialized successfully\n");
    kprintf("========================\n\n");
}

void gdt_set_kernel_stack(uint64_t stack_top)
{
    tss.rsp0 = stack_top;
}

void gdt_update_for_higher_half(void) {
    gdt_ptr.base = (uint64_t)phys_to_virt(virt_to_phys(&gdt));
    
    // Reload the GDTR register with the new virtual address
    __asm__ volatile("lgdt %0" : : "m"(gdt_ptr));
}
