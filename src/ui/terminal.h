/*
 * terminal.h
 * Main printing header file for URIX
 */
#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include <stddef.h>

// VGA text mode constants
#define VGA_WIDTH 80       // width of screen in characters
#define VGA_HEIGHT 25      // height of screen in lines
#define VGA_MEMORY 0xB8000 // memory-mapped address of VGA text buffer

// VGA color constants
typedef enum
{
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
} vga_color_t;

/*
* the function takes the foreground and background and puts them in the same byte for vga, using bitwise operations
*/
uint8_t vga_entry_color(vga_color_t fg, vga_color_t bg);

/*
* takes a character and color in the vga format and returns a 16 bit vga entry so it can be displayed on the screen
*/
uint16_t vga_entry(unsigned char uc, uint8_t color);

/*
* takes a string and returns its length in size_t (unsigned long)
*/
size_t strlen(const char *str);

/*
* removes all the characters from the screen, initializes the screen with spaces.
*/
void terminal_initialize(void);

/*
* takes a color made using the vga standard and updates the color of the screen.
*/
void terminal_setcolor(uint8_t color);

/*
* takes a character, color and placement (x and y coordinates) and puts it on the screen
*/
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y);

/*
* moves all the text on screen by one line and removes the text on the last line at the bottom
*/
void terminal_scroll(void);

/*
* adds one character to the screen, handles \n \t and \r
* moves a line down if the line is too long (longer than the screen size)
*/
void terminal_putchar(char c);

/*
* takes a string and its size and puts it on the string in the correct place
*/
void terminal_write(const char *data, size_t size);

/*
* takes a string, and prints onto the screen from where the string was last
*/
void terminal_writestring(const char *data);

/*
* takes an unsigend 64 bit integer and prints it onto the screen correctly
*/
void print_uint64(uint64_t value);

/*
* takes an unsigned 64 bit integer and prints it onto the screen in hexadecimal (base 16)
*/
void print_hex(uint64_t value);

/*
* takes a number, a character buffer (string) and a base (like 2, 10 and 16), and returns a string value of the number based on the base
* it also puts the same number in the buffer it takes
*/
char *itoa(size_t num, char *buffer, size_t base);

#endif