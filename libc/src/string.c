/*
 * Licensed under MIT License - URIX project.
 * string.c - minimal string and number conversion utilities.
 */

#include <stdint.h>
#include <stddef.h>

size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

char *strcpy(char *dest, const char *src)
{
    if (!dest || !src)
        return NULL;

    char *ret = dest;
    while ((*dest++ = *src++))
        ;

    return ret;
}

char *strchr(const char *s, int c)
{
    while (1)
    {
        if (*s == (char)c)
        {
            return (char *)s;
        }
        if (*s == '\0')
        {
            return NULL;
        }
        s++;
    }
}

void reverse(char *str, size_t len)
{
    for (size_t i = 0; i < len / 2; i++)
    {
        char tmp = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = tmp;
    }
}

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

char *strncpy(char *dest, const char *src, size_t n)
{
    if (!dest || !src)
        return NULL;

    size_t i = 0;

    /* Copy until end of src or n bytes */
    /* n-1 ensures that at the end of the buffer we will have a \0 terminated string */
    for (; i < n - 1 && src[i]; i++)
        dest[i] = src[i];

    /* Zero-pad remainder */
    for (; i < n; i++)
        dest[i] = '\0';

    return dest;
}

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

void *memcpy(void *dest, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    while (n--)
    {
        *d++ = *s++;
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    if (d < s)
    {
        // Copy forward
        while (n--)
        {
            *d++ = *s++;
        }
    }
    else
    {
        // Copy backward
        d += n;
        s += n;
        while (n--)
        {
            *--d = *--s;
        }
    }
    return dest;
}

char *strcat(char *dest, const char *src)
{
    char *ptr = dest;

    // Move ptr to the end of the destination string
    while (*ptr != '\0')
    {
        ptr++;
    }

    // Copy source string to the end of destination
    while (*src != '\0')
    {
        *ptr = *src;
        ptr++;
        src++;
    }

    // Null-terminate the result
    *ptr = '\0';

    return dest;
}

long strtol(const char *str, char **endptr, int base)
{
    if (!str)
    {
        if (endptr) *endptr = NULL;
        return 0;
    }

    // Skip leading whitespace
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r' || *str == '\f' || *str == '\v')
        str++;

    // Handle sign
    int sign = 1;
    if (*str == '-')
    {
        sign = -1;
        str++;
    }
    else if (*str == '+')
    {
        str++;
    }

    // Determine base
    if (base == 0)
    {
        if (*str == '0')
        {
            str++;
            if (*str == 'x' || *str == 'X')
            {
                base = 16;
                str++;
            }
            else
            {
                base = 8;
            }
        }
        else
        {
            base = 10;
        }
    }
    else if (base < 2 || base > 36)
    {
        // Invalid base
        if (endptr) *endptr = (char *)str;
        return 0;
    }

    // Convert
    long result = 0;
    int digit;
    int valid = 0;

    while (*str)
    {
        if (*str >= '0' && *str <= '9')
            digit = *str - '0';
        else if (*str >= 'a' && *str <= 'z')
            digit = *str - 'a' + 10;
        else if (*str >= 'A' && *str <= 'Z')
            digit = *str - 'A' + 10;
        else
            break;

        if (digit >= base)
            break;

        result = result * base + digit;
        valid = 1;
        str++;
    }

    if (!valid)
    {
        // No valid digits
        if (endptr) *endptr = (char *)str;
        return 0;
    }

    if (endptr) *endptr = (char *)str;

    return sign * result;
}

int atoi(const char *str)
{
    return (int)strtol(str, NULL, 10);
}

long atol(const char *str)
{
    return strtol(str, NULL, 10);
}