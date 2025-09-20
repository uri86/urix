/*
 * vga.c
 * VGA driver file for URIX
 */

#include <drivers/vga.h>
#include <lib/string.h>

// state
static size_t console_row = 0;
static size_t console_column = 0;
static uint8_t console_color = 0;
static volatile uint16_t *console_buffer = (volatile uint16_t *)VGA_MEMORY;

/**
 * vga_entry_color - combine fg/bg colors into one byte
 */
uint8_t vga_entry_color(vga_color_t fg, vga_color_t bg)
{
    return fg | (bg << 4);
}

/**
 * vga_entry - combine char + color into VGA cell
 */
uint16_t vga_entry(unsigned char uc, uint8_t color)
{
    return (uint16_t)uc | ((uint16_t)color << 8);
}

/**
 * console_initialize - clear screen and reset cursor
 */
void console_initialize(void)
{
    console_row = 0;
    console_column = 0;
    console_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    for (size_t y = 0; y < VGA_HEIGHT; y++)
    {
        for (size_t x = 0; x < VGA_WIDTH; x++)
        {
            console_buffer[y * VGA_WIDTH + x] = vga_entry(' ', console_color);
        }
    }
}

/**
 * console_set_color - set current text color
 */
void console_set_color(uint8_t color)
{
    console_color = color;
}

/**
 * console_putentryat - put character at (x,y)
 */
void console_putentryat(char c, uint8_t color, size_t x, size_t y)
{
    console_buffer[y * VGA_WIDTH + x] = vga_entry(c, color);
}

/**
 * console_scroll_up - scroll text up one row
 */
void console_scroll_up(void)
{
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
    {
        for (size_t x = 0; x < VGA_WIDTH; x++)
        {
            console_buffer[y * VGA_WIDTH + x] = console_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }

    for (size_t x = 0; x < VGA_WIDTH; x++)
    {
        console_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', console_color);
    }
}

/**
 * console_putchar - print char at cursor, handle special chars
 */
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
        console_scroll_up();
        console_row = VGA_HEIGHT - 1;
    }
}

/**
 * console_write - write buffer of size N
 */
void console_write(const char *data, size_t size)
{
    for (size_t i = 0; i < size; i++)
        console_putchar(data[i]);
}

/**
 * console_writestring - write null-terminated string
 */
void console_writestring(const char *data)
{
    console_write(data, strlen(data));
}

/**
 * console_clear - clear screen
 */
void console_clear(void)
{
    console_initialize();
}

/**
 * console_puts - alias for writestring
 */
void console_puts(const char *str)
{
    console_writestring(str);
}
