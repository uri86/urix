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
 * strlen - Return the length of a null-terminated string.
 *
 * str: Pointer to the string.
 *
 * Returns the number of characters before the first null byte.
 * Does not perform bounds checking—assumes valid and terminated input.
 */
size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

/**
 * reverse - Reverse a string in place.
 *
 * str: Pointer to the string buffer.
 * len: Number of characters to reverse.
 *
 * Used primarily by numeric conversion functions.
 * Caller must ensure the buffer is writable and properly sized.
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
 * itoa - Convert signed integer to string using a given base.
 *
 * num:   Signed 64-bit integer to convert.
 * buffer: Output character buffer (must be large enough).
 * base:  Numeral base (valid range: 2–36).
 *
 * Returns buffer containing a null-terminated string representation.
 * Supports negative values only in base 10.
 * If base is invalid, writes an empty string.
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
 * utoa - Convert unsigned integer to string using a given base.
 *
 * num:   Unsigned 64-bit integer to convert.
 * buffer: Output character buffer (must be large enough).
 * base:  Numeral base (valid range: 2–36).
 *
 * Returns buffer containing a null-terminated string representation.
 * If base is invalid, writes an empty string.
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

/**
 * strcmp - Compare two null-terminated strings.
 *
 * s1: Pointer to the first string.
 * s2: Pointer to the second string.
 *
 * Returns:
 *   0  -> strings are equal
 *   >0 -> the first differing character in s1 is greater than that in s2
 *   <0 -> the first differing character in s1 is less than that in s2
 *   -1 -> one or both input pointers are NULL
 *
 * Notes / Known Limitations:
 *   - This function explicitly checks for NULL pointers and returns -1 if either
 *     string is NULL, rather than causing undefined behavior.
 *   - Both strings must be correctly null-terminated. If either is not, this
 *     function may access memory out of bounds, which can lead to a crash or
 *     page fault.
 *   - No maximum read length is enforced. If termination is not guaranteed, consider
 *     using a length-limited version (e.g., strncmp) to prevent potential infinite
 *     loops or invalid memory access.
 *   - The comparison uses unsigned char arithmetic for the return value, which is
 *     standard behavior but may give unexpected results with non-ASCII characters.
 *
 * Use only when you are certain the input strings are properly null-terminated.
 */
int strcmp(const char *s1, const char *s2)
{
    if (!s1 || !s2)
        return -1;
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

/**
 * strncmp - Compare up to n characters of two null-terminated strings.
 *
 * s1: Pointer to the first string.
 * s2: Pointer to the second string.
 * n:  Maximum number of characters to compare.
 *
 * Returns:
 *   0  -> the compared portions of both strings are equal
 *   >0 -> the first differing character in s1 is greater than that in s2
 *   <0 -> the first differing character in s1 is less than that in s2
 *   -1 -> one or both input pointers are NULL
 *
 * Notes / Known Limitations:
 *   - If either pointer is NULL, returns -1 to prevent undefined behavior.
 *   - Comparison stops when:
 *        1. A differing character is found
 *        2. A null terminator ('\0') is reached
 *        3. n characters have been compared
 *   - Unlike strcmp, this function reduces risk of invalid memory access by
 *     enforcing a maximum read length. However, if n is too large and the string
 *     is not null-terminated within that range, it may still read unintended memory.
 *   - The return value is based on the difference between the unsigned char
 *     representations of the differing characters.
 *
 */
int strncmp(const char *s1, const char *s2, size_t n)
{
    if (!s1 || !s2)
        return -1;

    while (n > 0 && *s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
        n--;
    }

    if (n == 0)
        return 0; // No differences found within limit

    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

/**
 * strncpy - Copy a fixed number of characters from one string to another.
 *
 * dest: Destination buffer.
 * src:  Source string.
 * n:    Maximum number of bytes to copy.
 *
 * Returns:
 *   Pointer to dest.
 *   If either dest or src is NULL, returns NULL.
 *
 * Semantics:
 *   - Copies up to n bytes from src to dest.
 *   - If src is shorter than n bytes, the remaining bytes in dest
 *     are filled with '\0'.
 *   - If src is equal to or longer than n bytes, dest will NOT be
 *     null-terminated.
 *
 * Notes / Known Limitations:
 *   - This function does NOT guarantee null-termination.
 *   - Caller must ensure dest has space for at least n bytes.
 *   - Designed for fixed-size buffers and kernel-safe copying.
 */
char *strncpy(char *dest, const char *src, size_t n)
{
    if (!dest || !src)
        return NULL;

    size_t i = 0;

    /* Copy until end of src or n bytes */
    /* n-1 ensures that at the end of the buffer we will have a \0 terminated string */
    for (; i < n-1 && src[i]; i++)
        dest[i] = src[i];

    /* Zero-pad remainder */
    for (; i < n; i++)
        dest[i] = '\0';

    return dest;
}


/**
 * memset - Fill a block of memory with a byte value.
 *
 * dest: Pointer to destination memory.
 * value: Byte value to write (converted to uint8_t).
 * count: Number of bytes to set.
 *
 * Writes value into count bytes starting from dest.
 * Returns dest.
 */
void *memset(void *s, int c, size_t n)
{
    uint8_t *ptr = (uint8_t *)s;
    uint8_t value = (uint8_t)c;

    // Handle small sizes byte-by-byte
    if (n < 8)
    {
        for (size_t i = 0; i < n; i++)
            ptr[i] = value;
        return s;
    }

    // Align to 8-byte boundary
    while (((uint64_t)ptr & 7) && n)
    {
        *ptr++ = value;
        n--;
    }

    // Fill 8 bytes at a time
    if (n >= 8)
    {
        uint64_t pattern = (uint64_t)value;
        pattern |= pattern << 8;
        pattern |= pattern << 16;
        pattern |= pattern << 32;

        uint64_t *ptr64 = (uint64_t *)ptr;
        while (n >= 8)
        {
            *ptr64++ = pattern;
            n -= 8;
        }
        ptr = (unsigned char *)ptr64;
    }

    // Handle remaining bytes
    while (n--)
        *ptr++ = value;

    return s;
}