/*
 * Licensed under MIT License - URIX project.
 * printf.c - Shared libc printf implementation handling.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

/**
 * vsnprintf - Format strings into a bounded buffer
 */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    size_t i = 0;
    while (*fmt && i < size - 1)
    {
        if (*fmt != '%')
        {
            buf[i++] = *fmt++;
            continue;
        }

        fmt++; // skip '%'

        if (*fmt == '%')
        {
            buf[i++] = *fmt++;
            continue;
        }

        // Parse flags
        int left_align = 0;
        char pad_char = ' ';
        if (*fmt == '-')
        {
            left_align = 1;
            fmt++;
        }
        else if (*fmt == '0')
        {
            pad_char = '0';
            fmt++;
        }

        // parse width
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9')
        {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        // parse length modifier
        int is_long = 0, is_long_long = 0;
        if (*fmt == 'l')
        {
            is_long = 1;
            fmt++;
            if (*fmt == 'l')
            {
                is_long_long = 1;
                fmt++;
            }
        }

        char tmp[64];
        char *str = tmp;
        size_t len = 0;

        // parse specifier
        switch (*fmt)
        {
        case 'd':
        case 'i':
        {
            int64_t val = is_long_long ? va_arg(args, int64_t) : (is_long ? va_arg(args, long) : va_arg(args, int));
            itoa((uint64_t)val, tmp, 10);
            len = strlen(tmp);
            break;
        }
        case 'u':
        {
            uint64_t val = is_long_long ? va_arg(args, uint64_t) : (is_long ? va_arg(args, unsigned long) : va_arg(args, unsigned int));
            utoa(val, tmp, 10);
            len = strlen(tmp);
            break;
        }
        case 'x':
        case 'X':
        {
            uint64_t val = is_long_long ? va_arg(args, uint64_t) : (is_long ? va_arg(args, unsigned long) : va_arg(args, unsigned int));
            utoa(val, tmp, 16);
            len = strlen(tmp);
            break;
        }
        case 'p':
        {
            uint64_t val = (uint64_t)va_arg(args, void *);
            tmp[0] = '0';
            tmp[1] = 'x';
            utoa(val, tmp + 2, 16);
            len = strlen(tmp);
            break;
        }
        case 'c':
        {
            tmp[0] = (char)va_arg(args, int);
            tmp[1] = '\0';
            len = 1;
            break;
        }
        case 's':
        {
            str = va_arg(args, char *);
            if (!str)
                str = "(null)";
            len = strlen(str);
            break;
        }
        default:
            // unsupported specifier
            // prints the literal string
            tmp[0] = '%';
            tmp[1] = *fmt;
            tmp[2] = '\0';
            len = 2;
            break;
        }

        int pad_len = width > (int)len ? width - (int)len : 0;

        // right align (padding first)
        if (!left_align)
        {
            while (pad_len > 0 && i < size - 1)
            {
                buf[i++] = pad_char;
                pad_len--;
            }
        }

        // insert string data
        for (size_t j = 0; j < len && i < size - 1; j++)
        {
            buf[i++] = str[j];
        }

        // left Align
        if (left_align)
        {
            while (pad_len > 0 && i < size - 1)
            {
                buf[i++] = ' ';
                pad_len--;
            }
        }

        if (*fmt)
            fmt++;
    }
    buf[i] = '\0';
    return (int)i;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return ret;
}
