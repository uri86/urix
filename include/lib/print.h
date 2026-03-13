/*
 * Licensed under MIT License - URIX project.
 * print.h - Console printing interface for URIX.
 */

#ifndef PRINT_H
#define PRINT_H

#include <drivers/vga.h>
#include <stdarg.h>

/*
 * debug_kprintf - a simple function enclosure for kprintf that only runs when the debug option is selected.
 */
void debug_kprintf(const char *fmt, ...);

/*
 * takes a string with formatting and prints it correctly
 */
void kprintf(const char *fmt, ...);

/*
 * wraps the function "screen_initialize"
 */
void clear_screen(void);

/*
 * takes two vga colors, background and foreground, and sets the output to the screen in the correct color
 */
void set_color(vga_color_t fg, vga_color_t bg);

/*
 * takes an unsigend 64 bit integer and prints it onto the screen correctly
 */
void print_uint64(uint64_t value);

/*
 * takes an unsigned 64 bit integer and prints it onto the screen in hex
 */
void print_hex(uint64_t value);

#endif