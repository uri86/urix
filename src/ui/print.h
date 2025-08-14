/*
* print.h
* holds all the functions needed to print to the screen (encapsulates terminal.h)
*/
#ifndef PRINT_H
#define PRINT_H

#include "./terminal.h"
#include <stdarg.h>

/*
* takes a string with formatting and prints it correctly
* uses functions from terminal.c
*/
void print(char *str, ...);

/*
* wraps the function "screen_initialize" from terminal.c 
*/
void clear_screen(void);

/*
* takes two vga colors, background and foreground, and sets the output to the screen in the correct color
*/
void set_color(vga_color_t bg, vga_color_t fg);

/*
* takes a string, it's length, a pointer to another string and a list of arguments.
* takes the original string the places the data from the arguments list into the string in the correct places.
* saves the new string into the fmt string pointer
*/
void kvsnprintf(char *buf, size_t size, const char *fmt, va_list args);

#endif