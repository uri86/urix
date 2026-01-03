/*
 * Licensed under MIT License - URIX project.
 * panic.c - Implementation of the kernel panic handler.
 */

#include <lib/panic.h>
#include <lib/print.h>

void panic(const char *message, const char *file, uint32_t line)
{
    /* Disable Interrupts (CLI) */
    __asm__ volatile("cli");

    /* Visual Indication
     * Switch to White text on Red background for high visibility.
     * Uses your existing VGA constants.
     */
    set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);

    /* Print the Panic Message */
    kprintf("\n\n!!! KERNEL PANIC !!!\n");
    kprintf("--------------------------------------------------\n");
    kprintf("Reason: %s\n", message);
    kprintf("Source: %s\n", file);
    kprintf("Line:   %u\n", line);
    kprintf("--------------------------------------------------\n");
    kprintf("System Halted.\n");

    /* Halt the CPU */
    for (;;)
    {
        __asm__ volatile("hlt");
    }
}