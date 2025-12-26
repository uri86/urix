#include <stdint.h>
#include <stddef.h>
#include <lib/print.h>
#include <lib/utils.h>
#include <lib/logo.h>
#include <lib/string.h>
#include <memory/physical/pmm.h>
#include <memory/vmm.h>
#include <interrupts/idt.h>
#include <interrupts/pic.h>
#include <cpu/gdt.h>

extern char _kernel_start;
extern char _kernel_end;

static inline uint64_t align_up(uint64_t x, uint64_t a) { return (x + a - 1) & ~(a - 1); }

static inline uint64_t read_cr3(void)
{
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

void enable_sse(void)
{
    uint64_t cr0, cr4;

    // Read CR0
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));

    // Clear EM (bit 2) - no x87 emulation
    // Set MP (bit 1) - monitor coprocessor
    cr0 &= ~(1ULL << 2); // Clear EM
    cr0 |= (1ULL << 1);  // Set MP

    // Write back CR0
    __asm__ volatile("mov %0, %%cr0" ::"r"(cr0));

    // Read CR4
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));

    // Set OSFXSR (bit 9) - enable SSE
    // Set OSXMMEXCPT (bit 10) - enable SSE exceptions
    cr4 |= (1ULL << 9);  // OSFXSR
    cr4 |= (1ULL << 10); // OSXMMEXCPT

    // Write back CR4
    __asm__ volatile("mov %0, %%cr4" ::"r"(cr4));
}

void kernel_main(uint64_t mb_info_addr)
{
    uint8_t *cmd = NULL;
    multiboot_size_tag *tag = (multiboot_size_tag *)(uintptr_t)mb_info_addr;
    multiboot_tag *t = (multiboot_tag *)((uint8_t *)mb_info_addr + 8);

    while (t->type != MULTIBOOT_TAG_TYPE_END)
    {
        if (t->type == MULTIBOOT_TAG_TYPE_BCL)
        {
            multiboot_tag_bcl *cmd_tag = (multiboot_tag_bcl *)t;
            cmd = cmd_tag->string;
        }
        t = (multiboot_tag *)((uint8_t *)t + ((t->size + 7) & ~7));
    }

    if (cmd && strcmp((char *)cmd, "debug") == 0)
    {
        debug_mode = 1;
    }

    clear_screen();
    print_logo();

    /* Initialize GDT */
    debug_kprintf("Initializing GDT...\n");
    gdt_init();

    /* Initialize IDT */
    debug_kprintf("Initializing IDT...\n");
    idt_init();

    /* Initialize PIC */
    debug_kprintf("Initializing PIC...\n");
    pic_init(0x20, 0x28);

    /* Mask all IRQs except timer */
    for (int i = 1; i < 16; i++)
    {
        pic_mask_irq(i);
    }
    pic_unmask_irq(0);

    enable_sse();

    /* Initialize PMM - this also creates identity mapping and switches CR3 */
    debug_delay_ms = 150;
    debug_kprintf("Initializing PMM...\n");
    pmm_init(tag);

    debug_kprintf("Initializing VMM...\n");
    vmm_init();

    console_update_address();

    debug_kprintf("Reloading GDT/IDT to higher half...\n");
    gdt_update_for_higher_half();
    idt_update_for_higher_half();
    debug_kprintf("Finished reloading of GDT/IDT...\n");
    vmm_finish_init();

    kprintf("Kernel is now fully virtual!\n");
    for (;;)
        __asm__ volatile("hlt");
}