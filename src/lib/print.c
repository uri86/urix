/*
 * print.c
 * encapsulates the console.c file into prettier functions
 */

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <lib/print.h>
#include <lib/string.h>

// track whether a default color has been set
static int color_initialized = 0;

/**
 * kprintf - printf-style console output
 */
void kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char buf[512]; // temp buffer

    if (!color_initialized)
    {
        set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
        color_initialized = 1;
    }

    kvsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    console_writestring(buf);
}

/**
 * kvsnprintf - printf core (minimal subset)
 */
void kvsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    size_t i = 0;

    for (; *fmt && i < size - 1; fmt++)
    {
        if (*fmt == '%')
        {
            fmt++; // skip '%'

            if (*fmt == 'd') // signed decimal
            {
                int value = va_arg(args, int);
                char tmp[32];
                itoa(value, tmp, 10);
                for (char *p = tmp; *p && i < size - 1; p++)
                    buf[i++] = *p;
            }
            else if (*fmt == 'u') // unsigned decimal
            {
                unsigned int value = va_arg(args, unsigned int);
                char tmp[32];
                utoa(value, tmp, 10);
                for (char *p = tmp; *p && i < size - 1; p++)
                    buf[i++] = *p;
            }
            else if (*fmt == 's') // string
            {
                char *str = va_arg(args, char *);
                if (!str)
                    str = "(null)";
                while (*str && i < size - 1)
                    buf[i++] = *str++;
            }
            else if (*fmt == 'x' || *fmt == 'X') // hex
            {
                unsigned int value = va_arg(args, unsigned int);
                char tmp[32];
                utoa(value, tmp, 16);
                for (char *p = tmp; *p && i < size - 1; p++)
                    buf[i++] = *p;
            }
            else if (*fmt == 'b') // binary
            {
                unsigned int value = va_arg(args, unsigned int);
                char tmp[64];
                utoa(value, tmp, 2);
                for (char *p = tmp; *p && i < size - 1; p++)
                    buf[i++] = *p;
            }
            else if (*fmt == 'l') // long/long long
            {
                fmt++;
                if (*fmt == 'l' && *(fmt + 1) == 'u') // "llu"
                {
                    fmt++; // consume second 'l'
                    fmt++; // consume 'u'
                    uint64_t value = va_arg(args, uint64_t);
                    char tmp[64];
                    utoa(value, tmp, 10);
                    for (char *p = tmp; *p && i < size - 1; p++)
                        buf[i++] = *p;
                }
            }
            else // unknown specifier → print literally
            {
                buf[i++] = '%';
                buf[i++] = *fmt;
            }
        }
        else
        {
            buf[i++] = *fmt;
        }
    }

    buf[i] = '\0';
}

/**
 * set_color - set default fg/bg text color
 */
void set_color(vga_color_t fg, vga_color_t bg)
{
    color_initialized = 1;
    console_set_color(vga_entry_color(fg, bg));
}

/**
 * clear_screen - wipe display
 */
void clear_screen(void)
{
    console_initialize();
}

/**
 * print_uint64 - decimal 64-bit integer
 */
void print_uint64(uint64_t value)
{
    char buffer[32];
    utoa(value, buffer, 10);
    console_writestring(buffer);
}

/**
 * print_hex - hexadecimal 64-bit integer (with 0x prefix)
 */
void print_hex(uint64_t value)
{
    console_writestring("0x");

    char buffer[32];
    utoa(value, buffer, 16);
    console_writestring(buffer);
}
