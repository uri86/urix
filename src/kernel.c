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
#include <tests/test.h>
#include <process/process.h>
#include <lib/panic.h>

void enable_sse(void)
{
    uint64_t cr0, cr4;

    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2);
    cr0 |= (1ULL << 1);
    __asm__ volatile("mov %0, %%cr0" ::"r"(cr0));

    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9);
    cr4 |= (1ULL << 10);
    __asm__ volatile("mov %0, %%cr4" ::"r"(cr4));
}

static void test_suite_process(void)
{
    run_all_tests();
    process_exit(0);

    // Should never get here.
    for (;;)
        __asm__ volatile("hlt");
}

void kernel_main(uint64_t mb_info_addr)
{
    uint8_t test_mode = 0;
    uint8_t *cmd = NULL;
    multiboot_size_tag *tag = (multiboot_size_tag *)(uintptr_t)mb_info_addr;
    multiboot_tag *t = (multiboot_tag *)((uint8_t *)mb_info_addr + 8);

    while (t->type != MULTIBOOT_TAG_TYPE_END)
    {
        if (t->type == MULTIBOOT_TAG_TYPE_BCL)
            cmd = ((multiboot_tag_bcl *)t)->string;

        t = (multiboot_tag *)((uint8_t *)t + ((t->size + 7) & ~7));
    }

    if (cmd && strcmp((char *)cmd, "test") == 0)
    {
        test_mode = 1;
        debug_mode = 1;
        debug_delay_ms = 100;
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

    process_init();

    if (test_mode)
    {
        int pid = process_create(
            (uint64_t)test_suite_process,
            "test-suite",
            PRIORITY_HIGH,
            PROCESS_KERNEL);

        if (pid < 0)
            PANIC("Failed to create test-suite process");
    }

    pic_unmask_irq(0);
    __asm__ volatile("sti");

    process_schedule();

    PANIC("Returned from scheduler in test mode");
}
