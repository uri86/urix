/*
 * print.c
 * encapsulates the terminal.c file into prettier functions
 */

#include <stdarg.h>
#include "./print.h"

static size_t isSet = 0;

void print(char *msg, ...)
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
    terminal_writestring(buf); // writes line to the screen.
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

void set_color(vga_color_t bg, vga_color_t fg)
{
    isSet = 1;                                  // set the value to true
    terminal_setcolor(vga_entry_color(fg, bg)); // changes the color to the given color
}

void clear_screen(void)
{
    terminal_initialize(); // clear screen and make it blank
}