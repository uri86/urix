/*
 * Licensed under MIT License - URIX project.
 * string.c - minimal string and number conversion utilities.
 * Responsibilities:
 *  - provide strlen() implementation
 *  - implement integer/string conversions (itoa, utoa)
 *  - reverse strings in-place (helper for conversions)
 * Notes:
 *  - only implements minimal subset needed by kernel
 *  - integer conversions support bases 2–36
 *  - designed for use in printf-style functions
 */

#include <stdint.h>
#include <stddef.h>
#include <lib/print.h>
#include <memory/physical/pmm.h>

/**
 * strlen - return length of null-terminated string
 */
size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

/**
 * reverse - helper: reverse string in place
 */
void reverse(char *str, size_t len)
{
    for (size_t i = 0; i < len / 2; i++)
    {
        char tmp = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = tmp;
    }
}

/**
 * itoa - signed integer to string
 */
char *itoa(int64_t num, char *buffer, int base)
{
    if (base < 2 || base > 36)
    {
        buffer[0] = '\0';
        return buffer;
    }

    size_t i = 0;
    int isNegative = 0;

    if (num == 0)
    {
        buffer[i++] = '0';
        buffer[i] = '\0';
        return buffer;
    }

    if (num < 0 && base == 10)
    {
        isNegative = 1;
        num = -num;
    }

    while (num > 0)
    {
        int digit = num % base;
        buffer[i++] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        num /= base;
    }

    if (isNegative)
        buffer[i++] = '-';

    buffer[i] = '\0';
    reverse(buffer, i);

    return buffer;
}

/**
 * utoa - unsigned integer to string
 */
char *utoa(uint64_t num, char *buffer, int base)
{
    if (base < 2 || base > 36)
    {
        buffer[0] = '\0';
        return buffer;
    }

    size_t i = 0;

    if (num == 0)
    {
        buffer[i++] = '0';
        buffer[i] = '\0';
        return buffer;
    }

    while (num > 0)
    {
        int digit = num % base;
        buffer[i++] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        num /= base;
    }

    buffer[i] = '\0';
    reverse(buffer, i);

    return buffer;
}

/*
 * takes a destination pointer, value, and size to iterate on.
 * enters the value given into the given count of bytes in memory, starting at the dest pointer given.
 */
void *memset(void *s, int c, size_t n)
{
    unsigned char *ptr = (unsigned char *)s;
    unsigned char value = (unsigned char)c;
    kprintf("memset(%llx, %x, %llu)\n", (uint64_t)s, c, n);

    // Iterate through the memory block and set each byte
    for (size_t i = 0; i < n; i++)
    {
        //kprintf("Setting: %x\n", (unsigned int)ptr[i]);

        ptr[i] = value;
    }

    return s; // Return the original pointer to the memory block
}
