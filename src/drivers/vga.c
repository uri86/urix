/*
 * Licensed under MIT License - URIX project.
 * vga.c - VGA text-mode driver implementation.
 * Responsibilities:
 *  - manage VGA text buffer at 0xB8000
 *  - handle colors, and screen state
 */

#include <drivers/vga.h>
#include <string.h>
#include <memory/vmm.h>
#include <ports.h>

#define CURSOR 219 // ASCII code for block character

// state
static size_t console_row = 0;
static size_t console_column = 0;
static uint8_t console_color = 0;
static int cursor_visible = 1;
static volatile uint16_t *console_buffer = (volatile uint16_t *)VGA_MEMORY;

/* ANSI sequence state machine variables */
static int ansi_state = 0;
static int ansi_param1 = 0;
static int ansi_param2 = 0;

static uint16_t old_cursor_pos = 0;
static uint16_t old_cursor_char_entry = 0;
static int is_cursor_drawn = 0;

static void remove_cursor(void)
{
    if (is_cursor_drawn)
    {
        console_buffer[old_cursor_pos] = old_cursor_char_entry;
        is_cursor_drawn = 0;
    }
}

static void draw_cursor(void)
{
    if (!is_cursor_drawn && cursor_visible)
    {
        old_cursor_pos = console_row * VGA_WIDTH + console_column;
        if (old_cursor_pos < VGA_WIDTH * VGA_HEIGHT)
        {
            old_cursor_char_entry = console_buffer[old_cursor_pos];
            console_buffer[old_cursor_pos] = vga_entry(CURSOR, console_color);
            is_cursor_drawn = 1;
        }
    }
}

static void update_cursor(void)
{
    remove_cursor();
    draw_cursor();
}

static void enable_cursor(uint8_t cursor_start, uint8_t cursor_end)
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}

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
    if (!console_color)
        console_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    remove_cursor();
    for (size_t y = 0; y < VGA_HEIGHT; y++)
    {
        for (size_t x = 0; x < VGA_WIDTH; x++)
        {
            console_buffer[y * VGA_WIDTH + x] = vga_entry(' ', console_color);
        }
    }
    console_row = 0;
    console_column = 0;
    update_cursor();
    enable_cursor(0x20, 0);
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
 * console_putchar - print char at cursor, handle special chars and ANSI sequences
 */
void console_putchar(char c)
{
    remove_cursor();

    // ANSI sequence fast-path
    if (ansi_state == 0)
    {
        if (c == '\x1b')
        {
            ansi_state = 1;
            return;
        }
        else if (c == '\n') // newline: move to start of next row
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
        else if (c == '\b') // backspace: removes the last character and moves the buffer to the correct place.
        {
            if (console_column > 0)
            {
                console_column--;
                console_putentryat(' ', console_color, console_column, console_row);
            }
            else if (console_row > 0)
            {
                console_row--;
                console_column = VGA_WIDTH - 1;
                console_putentryat(' ', console_color, console_column, console_row);
            }
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

        update_cursor();
    }
    else if (ansi_state == 1)
    {
        if (c == '[')
        {
            ansi_state = 2;
            ansi_param1 = 0;
            ansi_param2 = 0;
        }
        else
        {
            ansi_state = 0; // abort
        }
    }
    else if (ansi_state == 2)
    {
        if (c == '?')
        {
            ansi_state = 4;
        }
        else if (c >= '0' && c <= '9')
        {
            ansi_param1 = ansi_param1 * 10 + (c - '0');
        }
        else if (c == ';')
        {
            ansi_state = 3;
        }
        else if (c == 'H' || c == 'f')
        {
            if (ansi_param1 > 0)
                ansi_param1--;
            console_row = ansi_param1;
            if (console_row >= VGA_HEIGHT)
                console_row = VGA_HEIGHT - 1;
            console_column = 0;
            update_cursor();
            ansi_state = 0;
        }
        else if (c == 'J')
        {
            if (ansi_param1 == 2)
            {
                console_clear();
            }
            ansi_state = 0;
        }
        else
        {
            ansi_state = 0;
        }
    }
    else if (ansi_state == 3)
    {
        if (c >= '0' && c <= '9')
        {
            ansi_param2 = ansi_param2 * 10 + (c - '0');
        }
        else if (c == 'H' || c == 'f')
        {
            if (ansi_param1 > 0)
                ansi_param1--;
            if (ansi_param2 > 0)
                ansi_param2--;
            console_row = ansi_param1;
            console_column = ansi_param2;
            if (console_row >= VGA_HEIGHT)
                console_row = VGA_HEIGHT - 1;
            if (console_column >= VGA_WIDTH)
                console_column = VGA_WIDTH - 1;
            update_cursor();
            ansi_state = 0;
        }
        else
        {
            ansi_state = 0;
        }
    }
    else if (ansi_state == 4)
    {
        if (c >= '0' && c <= '9')
        {
            ansi_param1 = ansi_param1 * 10 + (c - '0');
        }
        else if (c == 'l')
        {
            if (ansi_param1 == 25)
            {
                cursor_visible = 0;
                update_cursor();
            }
            ansi_state = 0;
        }
        else if (c == 'h')
        {
            if (ansi_param1 == 25)
            {
                cursor_visible = 1;
                update_cursor();
            }
            ansi_state = 0;
        }
        else
        {
            ansi_state = 0;
        }
    }

    update_cursor();
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

/*
 * console_update_address - Update the VGA buffer to its virtual address
 */
void console_update_address(void)
{
    console_buffer = (volatile uint16_t *)phys_to_virt(VGA_MEMORY);
}