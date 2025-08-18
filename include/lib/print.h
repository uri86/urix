/*
* print.h
* holds all the functions needed to print to the screen, using the vga driver.
*/
#ifndef PRINT_H
#define PRINT_H

#include <drivers/vga.h>
#include <stdarg.h>

/*
* takes a string with formatting and prints it correctly
* uses functions from terminal.c
*/
void kprintf(char *str, ...);

/*
* wraps the function "screen_initialize" from terminal.c 
*/
void clear_screen(void);

/*
* takes two vga colors, background and foreground, and sets the output to the screen in the correct color
*/
void set_color(vga_color_t fg, vga_color_t bg);

/*
* takes a string, it's length, a pointer to another string and a list of arguments.
* takes the original string the places the data from the arguments list into the string in the correct places.
* saves the new string into the fmt string pointer
*/
void kvsnprintf(char *buf, size_t size, const char *fmt, va_list args);

/*
* takes an unsigend 64 bit integer and prints it onto the screen correctly
*/
void print_uint64(uint64_t value);

/*
* takes an unsigned 64 bit integer and prints it onto the screen in hexadecimal (base 16)
*/
void print_hex(uint64_t value);

#endif