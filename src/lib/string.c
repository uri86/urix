#include <stdint.h>

// simple string length function (like strlen in stdlib)
uint64_t strlen(const char *str)
{
    uint64_t len = 0;
    while (str[len])
        len++;
    return len;
}

// convert integer to string in specified base
char *itoa(uint64_t num, char *buffer, int base)
{
    uint64_t pos = 0;
    uint64_t isNegative = 0;

    // handle zero case
    if (num == 0)
    {
        buffer[0] = '0';
        buffer[1] = '\0';
        return buffer;
    }

    // handle negative numbers (only for base 10)
    if (num < (uint64_t)0 && base == 10)
    {
        isNegative = 1;
        num = -num; // Note: This can overflow for INT_MIN
    }

    // convert number to string in reverse order
    while (num > 0)
    {
        uint64_t digit = num % base;
        if (digit < 10)
            buffer[pos++] = '0' + digit;
        else
            buffer[pos++] = 'A' + digit - 10;
        num /= base;
    }

    // add negative sign if needed
    if (isNegative)
        buffer[pos++] = '-';

    // reverse the string to get correct order
    for (uint64_t i = 0; i < pos / 2; i++)
    {
        char temp = buffer[i];
        buffer[i] = buffer[pos - 1 - i];
        buffer[pos - 1 - i] = temp;
    }

    // null terminate
    buffer[pos] = '\0';

    return buffer;
}