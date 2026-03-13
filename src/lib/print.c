/*
 * Licensed under MIT License - URIX project.
 * print.c - Higher-level console printing utilities.
 * Responsibilities:
 *  - provide kprintf (printf-style formatted output)
 *  - wrap low-level VGA/console calls
 *  - support integers (signed/unsigned), hex, binary, strings
 *  - manage text color and screen clearing
 */

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <lib/print.h>
#include <string.h>
#include <lib/utils.h>
#include <stdio.h>
// track whether a default color has been set
static int color_initialized = 0;

void debug_kprintf(const char *fmt, ...)
{
    if (!debug_mode)
        return;

    va_list args;
    va_start(args, fmt);

    if (!color_initialized)
    {
        set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
        color_initialized = 1;
    }

    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, args);

    va_end(args);

    delay_ms(debug_delay_ms);

    console_writestring(buf);
}

/**
 * kprintf - printf-style console output
 */
void kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char buf[512]; // temp buffer

    if (!color_initialized)
    {
        set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
        color_initialized = 1;
    }

    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    console_writestring(buf);

    delay_ms(debug_delay_ms);
}



/**
 * set_color - set default fg/bg text color
 */
void set_color(vga_color_t fg, vga_color_t bg)
{
    color_initialized = 1;
    console_set_color(vga_entry_color(fg, bg));
}

/**
 * clear_screen - wipe display
 */
void clear_screen(void)
{
    console_initialize();
}

/**
 * print_uint64 - decimal 64-bit integer
 */
void print_uint64(uint64_t value)
{
    char buffer[32];
    utoa(value, buffer, 10);
    console_writestring(buffer);
}

/**
 * print_hex - hexadecimal 64-bit integer (with 0x prefix)
 */
void print_hex(uint64_t value)
{
    console_writestring("0x");

    char buffer[32];
    utoa(value, buffer, 16);
    console_writestring(buffer);
}
