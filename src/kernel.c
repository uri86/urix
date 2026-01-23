#include <stdint.h>
#include <stddef.h>
#include <lib/print.h>
#include <lib/utils.h>
#include <lib/logo.h>
#include <lib/string.h>
#include <memory/physical/pmm.h>
#include <memory/vmm.h>
#include <memory/kmalloc.h>
#include <interrupts/idt.h>
#include <interrupts/pic.h>
#include <cpu/gdt.h>
#include <tests/test.h>
#include <process/process.h>
#include <lib/panic.h>
#include <fs/blockdev.h>
#include <fs/vfs.h>
#include <fs/ext2.h>
#include <fs/vfs_manager.h>
#include <drivers/ata.h>
#include <drivers/ramdisk.h>

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

void test_process(void)
{
    process_t *me = process_get_current();
    for (int i = 0; i <= 50; i++)
    {
        kprintf("[%s] (PID: %u) at: %d\n", me->name, me->pid, i);
        process_yield();
    }
    process_exit(0);
}

static void test_suite_process(void)
{
    run_all_tests();
    process_exit(0);
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

    if (cmd && strcmp((char *)cmd, "debug") == 0)
    {
        debug_mode = 1;
        debug_delay_ms = 150;
    }
    else if (cmd && strcmp((char *)cmd, "test") == 0)
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

    for (int i = 0; i < 16; i++)
        pic_mask_irq(i);

    enable_sse();
    debug_kprintf("Initializing PMM...\n");
    pmm_init(tag);

    debug_kprintf("Initializing VMM...\n");
    vmm_init();
    kmalloc_init();
    console_update_address();

    debug_kprintf("Reloading GDT/IDT to higher half...\n");
    gdt_update_for_higher_half();
    idt_update_for_higher_half();
    debug_kprintf("Finished reloading of GDT/IDT...\n");
    vmm_finish_init();

    if (!test_mode)
    {
        debug_kprintf("Initializing block device layer...\n");
        blockdev_init();

        debug_kprintf("Initializing ATA driver...\n");
        ata_init();

        /* Check if any real disks exist */
        if (blockdev_find("hda") == NULL &&
            blockdev_find("hdb") == NULL &&
            blockdev_find("hdc") == NULL &&
            blockdev_find("hdd") == NULL)
        {
            kprintf("[WARN] No ATA disks detected. Creating 16MB RAM disk...\n");

            blockdev_t *ramdisk = NULL;
            if (ramdisk_create("ramdisk0", 16, &ramdisk) != 0)
                PANIC("Failed to create RAM disk");
        }

        debug_kprintf("Initializing VFS core...\n");
        vfs_init();

        debug_kprintf("Initializing Ext2 driver...\n");
        ext2_init();

        debug_kprintf("Starting VFS Manager auto-scan...\n");
        if (vfs_manager_init() != 0)
            PANIC("VFS Manager failed to mount root filesystem");
    }

    process_init();

    if (test_mode)
    {
        if (process_create((uint64_t)test_suite_process, "test-suite", PRIORITY_HIGH, PROCESS_KERNEL) < 0)
            PANIC("Failed to create test-suite process");
    }
    else
    {
        process_create((uint64_t)test_process, "test1", PRIORITY_LOW, PROCESS_KERNEL);
        process_create((uint64_t)test_process, "test2", PRIORITY_LOW, PROCESS_KERNEL);
        process_create((uint64_t)test_process, "test3", PRIORITY_NORMAL, PROCESS_KERNEL);
        process_create((uint64_t)test_process, "test4", PRIORITY_NORMAL, PROCESS_KERNEL);
        process_create((uint64_t)test_process, "test5", PRIORITY_NORMAL, PROCESS_KERNEL);
        process_create((uint64_t)test_process, "test6", PRIORITY_HIGH, PROCESS_KERNEL);
    }

    pic_unmask_irq(0);
    __asm__ volatile("sti");

    process_schedule();

    PANIC("Returned from scheduler");
}