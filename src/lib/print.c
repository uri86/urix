/*
 * print.c
 * encapsulates the console.c file into prettier functions
 */

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <lib/print.h>
#include <lib/string.h>


static size_t isSet = 0;

void kprintf(char *msg, ...)
{
    va_list args;        // unknown amount of arguments
    va_start(args, msg); // loads the arguments
    char buf[512];       // let's you take a buffer of 512 characters

    if (!isSet)
    {
        set_color(VGA_COLOR_BLACK, VGA_COLOR_GREEN); // sets a default color
        isSet = 1;
    }

    kvsnprintf(buf, sizeof(buf), msg, args); // handles the use of string formatting (like printf)
    va_end(args);
    console_writestring(buf); // writes line to the screen.
}

void kvsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    size_t i = 0;
    for (; *fmt && i < size - 1; fmt++)
    {
        if (*fmt == '%')
        {
            fmt++;
            if (*fmt == 'd') // handle integer
            {
                size_t value = va_arg(args, int);
                char tmp[32];
                itoa(value, tmp, 10);
                for (char *p = tmp; *p && i < size - 1; p++)
                    buf[i++] = *p;
            }
            else if (*fmt == 's') // handle string
            {
                char *str = va_arg(args, char *);
                while (*str && i < size - 1)
                    buf[i++] = *str++;
            }
            else if (*fmt == 'x' || *fmt == 'X') // handle hexadecimal representation
            {
                size_t value = va_arg(args, int);
                char tmp[21];
                itoa(value, tmp, 16);
                for (char *p = tmp; *p && i < size - 1; p++)
                    buf[i++] = *p;
            }
            else if (*fmt == 'b') // handle binary representation
            {
                size_t value = va_arg(args, int);
                char tmp[21];
                itoa(value, tmp, 2);
                for (char *p = tmp; *p && i < size - 1; p++)
                    buf[i++] = *p;
            }
        }
        else
        {
            buf[i++] = *fmt;
        }
    }
    buf[i] = '\0';
}

void set_color(vga_color_t fg, vga_color_t bg)
{
    isSet = 1;                                  // set the value to true
    console_set_color(vga_entry_color(fg, bg)); // changes the color to the given color
}

void clear_screen(void)
{
    console_initialize(); // clear screen and make it blank
}


// print an unsigned 64-bit integer in decimal
void print_uint64(uint64_t value)
{
    if (value == 0)
    {
        console_putchar('0');
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
    for (int i = pos - 1; i >= 0; i--)
    {
        console_putchar(buffer[i]);
    }
}

// print an unsigned 64-bit integer in hexadecimal (prefixed with 0x)
void print_hex(uint64_t value)
{
    console_writestring("0x");

    if (value == 0)
    {
        console_putchar('0');
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
    for (int i = pos - 1; i >= 0; i--)
    {
        console_putchar(buffer[i]);
    }
}