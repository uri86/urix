/*
 * vga.h
 * VGA driver header file for URIX
 */
#ifndef VGA_H
#define VGA_H

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
* the function takes the foreground and background and puts them in the same byte for console, using bitwise operations
*/
uint8_t vga_entry_color(vga_color_t fg, vga_color_t bg);

/*
* takes a character and color in the console format and returns a 16 bit console entry so it can be displayed on the screen
*/
uint16_t vga_entry(unsigned char uc, uint8_t color);


/*
* removes all the characters from the screen, initializes the screen with spaces.
*/
void console_initialize(void);

/*
* takes a color made using the console standard and updates the color of the screen.
*/
void console_set_color(uint8_t color);

/*
* takes a character, color and placement (x and y coordinates) and puts it on the screen
*/
void console_putentryat(char c, uint8_t color, size_t x, size_t y);

/*
* moves all the text on screen by one line and removes the text on the last line at the bottom
*/
void console_scroll(void);

/*
* adds one character to the screen, handles \n \t and \r
* moves a line down if the line is too long (longer than the screen size)
*/
void console_putchar(char c);

/*
* takes a string and its size and puts it on the string in the correct place
*/
void console_write(const char *data, size_t size);

/*
* takes a string, and prints onto the screen from where the string was last
*/
void console_writestring(const char *data);


#endif