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
#include <drivers/keyboard.h>

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
    for (;;)
        __asm__ volatile("hlt");
}

void keyboard_test_process(void)
{
    kprintf("\n=== Interactive Keyboard Test ===\n");
    kprintf("Type 'help' for commands, 'exit' to quit\n\n");

    char buffer[256];
    while (1)
    {
        kprintf(" urix> ");
        size_t len = keyboard_gets(buffer, sizeof(buffer));

        if (len == 0)
            continue;

        if (strcmp(buffer, "exit") == 0 || strcmp(buffer, "quit") == 0)
        {
            kprintf("Exiting keyboard test...\n");
            break;
        }
        else if (strcmp(buffer, "help") == 0)
        {
            kprintf("Available commands:\n");
            kprintf("  help    - Show this help\n");
            kprintf("  echo    - Echo test\n");
            kprintf("  state   - Show keyboard state\n");
            kprintf("  clear   - Clear screen\n");
            kprintf("  ps      - Show process table\n");
            kprintf("  exit    - Exit keyboard test\n");
        }
        else if (strcmp(buffer, "echo") == 0)
        {
            kprintf("Echo mode - type something:\n");
            kprintf("> ");
            keyboard_gets(buffer, sizeof(buffer));
            kprintf("You typed: '%s'\n", buffer);
        }
        else if (strcmp(buffer, "state") == 0)
        {
            uint16_t state = keyboard_get_state();
            kprintf("Keyboard state: 0x%x\n", state);
            kprintf("  Shift: %s\n", (state & KB_SHIFT) ? "ON" : "OFF");
            kprintf("  Ctrl:  %s\n", (state & KB_CTRL) ? "ON" : "OFF");
            kprintf("  Alt:   %s\n", (state & KB_ALT) ? "ON" : "OFF");
            kprintf("  Caps:  %s\n", (state & KB_CAPS_LOCK) ? "ON" : "OFF");
            kprintf("  Num:   %s\n", (state & KB_NUM_LOCK) ? "ON" : "OFF");
        }
        else if (strcmp(buffer, "clear") == 0)
        {
            clear_screen();
        }
        else if (strcmp(buffer, "ps") == 0)
        {
            process_print_table();
        }
        else if (strcmp(buffer, "prntlg") == 0)
        {
            print_logo();
        }
        else
        {
            kprintf("Unknown command: '%s' (type 'help' for commands)\n", buffer);
        }
    }

    kprintf("Keyboard test finished.\n");
    process_exit(0);
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

    /* Initialize keyboard driver */
    debug_kprintf("Initializing keyboard driver...\n");
    keyboard_init();

    /* Initialize process manager */
    process_init();

    if (test_mode)
    {
        /* Test mode - run test suite */
        if (process_create((uint64_t)test_suite_process, "test-suite", PRIORITY_HIGH, PROCESS_KERNEL) < 0)
            PANIC("Failed to create test-suite process");
    }
    else
    {
        /* Default mode - keyboard test */
        kprintf("\n=== Starting Keyboard Test ===\n\n");
        if (process_create((uint64_t)keyboard_test_process, "kbd-test", PRIORITY_NORMAL, PROCESS_KERNEL) < 0)
            PANIC("Failed to create keyboard test process");
    }

    /* Unmask timer (IRQ0) and keyboard (IRQ1) */
    pic_unmask_irq(0);
    pic_unmask_irq(1);

    /* Enable interrupts */
    __asm__ volatile("sti");

    /* Start scheduler */
    process_schedule();

    PANIC("Returned from scheduler");
}