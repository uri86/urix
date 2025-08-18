/*
 * vga.c
 * VGA driver file for URIX
 */

#include <drivers/vga.h>
#include <lib/string.h>

// console state variables
static size_t console_row = 0;    // current row position
static size_t console_column = 0; // current column position
static uint8_t console_color = 0; // current color
static volatile uint16_t *console_buffer = (volatile uint16_t *)VGA_MEMORY;

// helper function to combine foreground and background colors into one byte
uint8_t vga_entry_color(vga_color_t fg, vga_color_t bg)
{
    return fg | bg << 4; // lower 4 bits = foreground, upper 4 bits = background
}

// helper function to combine character and color into a 16-bit VGA entry
uint16_t vga_entry(unsigned char uc, uint8_t color)
{
    return (uint16_t)uc | (uint16_t)color << 8;
}

// initialize console: clear screen
void console_initialize(void)
{
    console_row = 0;
    console_column = 0;
    console_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    // fill entire screen with spaces in the default color
    for (size_t y = 0; y < VGA_HEIGHT; y++)
    {
        for (size_t x = 0; x < VGA_WIDTH; x++)
        {
            const size_t index = y * VGA_WIDTH + x;
            console_buffer[index] = vga_entry(' ', console_color);
        }
    }
}

// change current text color
void console_set_color(uint8_t color)
{
    console_color = color;
}

// put a single character 'c' at (x, y) with given color
void console_putentryat(char c, uint8_t color, size_t x, size_t y)
{
    const size_t index = y * VGA_WIDTH + x;
    console_buffer[index] = vga_entry(c, color);
}

// scroll console up one line when bottom is reached
void console_scroll(void)
{
    // move all lines up by copying each line to the previous line
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
    {
        for (size_t x = 0; x < VGA_WIDTH; x++)
        {
            console_buffer[y * VGA_WIDTH + x] = console_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }

    // clear the last line to blank spaces
    for (size_t x = 0; x < VGA_WIDTH; x++)
    {
        console_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', console_color);
    }
}

// put a single character at the current cursor position
// handles '\n', '\r', '\t', wrapping, and scrolling
void console_putchar(char c)
{
    if (c == '\n') // newline: move to start of next row
    {
        console_column = 0;
        console_row++;
    }
    else if (c == '\r') // carriage return: move to start of current row
    {
        console_column = 0;
    }
    else if (c == '\t') // tab: advance to next 8-character boundary
    {
        console_column = (console_column + 8) & ~7;
    }
    else // normal character
    {
        console_putentryat(c, console_color, console_column, console_row);
        console_column++;
    }

    // wrap lines if end of row reached
    if (console_column >= VGA_WIDTH)
    {
        console_column = 0;
        console_row++;
    }

    // scroll if bottom of screen reached
    if (console_row >= VGA_HEIGHT)
    {
        console_scroll();
        console_row = VGA_HEIGHT - 1;
    }
}

// write a buffer of characters to console
void console_write(const char *data, size_t size)
{
    for (size_t i = 0; i < size; i++)
        console_putchar(data[i]);
}

// write null-terminated string to console
void console_writestring(const char *data)
{
    console_write(data, strlen(data));
}

