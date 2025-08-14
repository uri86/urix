/*
 * kernel.c - URIX 64-bit kernel
 * Main kernel entry point and basic VGA text output
 */
#include "./ui/print.h"
#include <stdarg.h>

// main kernel entry point, called from boot.S after entering 64-bit mode
void kernel_main(void)
{
    clear_screen();
    print("Welcome to URIX 64-bit!\n");
    print("Kernel successfully loaded in 64-bit mode.\n\n");
    set_color(VGA_COLOR_BLACK, VGA_COLOR_WHITE);
    terminal_writestring("Kernel successfully loaded in 64-bit mode.\n\n");
    set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    print("System Information:\n");
    print("- Architecture: x86_64\n");
    print("- Kernel: URIX\n");
    print("- VGA Text Mode: 80x25\n");
    print("- VGA Buffer: %x\n", (uint64_t)VGA_MEMORY);

    // show kernel stack address (prove 64-bit stack is active)
    print("- Kernel Stack: ");
    uint64_t stack_addr;
    __asm__ volatile("mov %%rsp, %0" : "=r"(stack_addr)); // read 64-bit RSP
    print("%d\n", stack_addr);

    // show that 64-bit arithmetic works
    print("- 64-bit test value: ");
    print_uint64(0x1234567890ABCDEF);
    terminal_putchar('\n');

    // print final message and halt
    set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    print("\nKernel is running. System halted.\n");

    // infinite loop halts CPU to prevent running into invalid memory
    for (;;)
    {
        __asm__ volatile("hlt"); // halt instruction, reduces CPU usage
    }
}

void kprintf(char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    print(fmt, args);
}