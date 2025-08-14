/*
 * terminal.c
 * Main printing file for URIX
 */

#include "./terminal.h"

// terminal state variables
static size_t terminal_row = 0;    // current row position
static size_t terminal_column = 0; // current column position
static uint8_t terminal_color = 0; // current color
static volatile uint16_t *terminal_buffer = (volatile uint16_t *)VGA_MEMORY;

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

// simple string length function (like strlen in stdlib)
size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

// initialize terminal: clear screen
void terminal_initialize(void)
{
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    // fill entire screen with spaces in the default color
    for (size_t y = 0; y < VGA_HEIGHT; y++)
    {
        for (size_t x = 0; x < VGA_WIDTH; x++)
        {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
}

// change current text color
void terminal_setcolor(uint8_t color)
{
    terminal_color = color;
}

// put a single character 'c' at (x, y) with given color
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y)
{
    const size_t index = y * VGA_WIDTH + x;
    terminal_buffer[index] = vga_entry(c, color);
}

// scroll terminal up one line when bottom is reached
void terminal_scroll(void)
{
    // move all lines up by copying each line to the previous line
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
    {
        for (size_t x = 0; x < VGA_WIDTH; x++)
        {
            terminal_buffer[y * VGA_WIDTH + x] = terminal_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }

    // clear the last line to blank spaces
    for (size_t x = 0; x < VGA_WIDTH; x++)
    {
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    }
}

// put a single character at the current cursor position
// handles '\n', '\r', '\t', wrapping, and scrolling
void terminal_putchar(char c)
{
    if (c == '\n') // newline: move to start of next row
    {
        terminal_column = 0;
        terminal_row++;
    }
    else if (c == '\r') // carriage return: move to start of current row
    {
        terminal_column = 0;
    }
    else if (c == '\t') // tab: advance to next 8-character boundary
    {
        terminal_column = (terminal_column + 8) & ~7;
    }
    else // normal character
    {
        terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
        terminal_column++;
    }

    // wrap lines if end of row reached
    if (terminal_column >= VGA_WIDTH)
    {
        terminal_column = 0;
        terminal_row++;
    }

    // scroll if bottom of screen reached
    if (terminal_row >= VGA_HEIGHT)
    {
        terminal_scroll();
        terminal_row = VGA_HEIGHT - 1;
    }
}

// write a buffer of characters to terminal
void terminal_write(const char *data, size_t size)
{
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

// write null-terminated string to terminal
void terminal_writestring(const char *data)
{
    terminal_write(data, strlen(data));
}

// print an unsigned 64-bit integer in decimal
void print_uint64(uint64_t value)
{
    if (value == 0)
    {
        terminal_putchar('0');
        return;
    }

    char buffer[21]; // enough to store 64-bit number
    size_t pos = 0;

    // convert number to string in reverse
    while (value > 0)
    {
        buffer[pos++] = '0' + (value % 10);
        value /= 10;
    }

    // print digits in correct order
    for (size_t i = pos - 1; i >= 0; i--)
    {
        terminal_putchar(buffer[i]);
    }
}

// print an unsigned 64-bit integer in hexadecimal (prefixed with 0x)
void print_hex(uint64_t value)
{
    terminal_writestring("0x");

    if (value == 0)
    {
        terminal_putchar('0');
        return;
    }

    char buffer[17]; // enough for 64-bit hex
    size_t pos = 0;

    // convert number to hex string in reverse
    while (value > 0)
    {
        size_t digit = value % 16;
        buffer[pos++] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        value /= 16;
    }

    // print digits in correct order
    for (size_t i = pos - 1; i >= 0; i--)
    {
        terminal_putchar(buffer[i]);
    }
}

// convert integer to string in specified base
char *itoa(size_t num, char *buffer, size_t base)
{
    size_t pos = 0;
    size_t isNegative = 0;
    char *result;

    // handle zero case
    if (num == 0)
    {
        result[0] = '0';
        result[1] = '\0';
        return;
    }

    // handle negative numbers (only for base 10)
    if (num < 0 && base == 10)
    {
        isNegative = 1;
        num = -num;
    }

    // convert number to string in reverse order
    while (num > 0)
    {
        size_t digit = num % base;

        if (digit < 10)
            result[pos++] = '0' + digit;
        else
            result[pos++] = 'A' + digit - 10;

        num /= base;
    }

    // add negative sign if needed
    if (isNegative)
        result[pos++] = '-';

    // reverse the string to get correct order
    for (size_t i = 0; i < pos / 2; i++)
    {
        char temp = result[i];
        result[i] = result[pos - 1 - i];
        result[pos - 1 - i] = temp;
    }

    // null terminate
    result[pos] = '\0';
    buffer = result;
    return result;
}