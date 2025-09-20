/*
 * string.h
 * holds all the functions for a string in URIX.
 */

#ifndef STRING_H
#define STRING_H

#include <stdint.h>

/*
 * takes a string and returns its length in size_t (unsigned long)
 */
uint64_t strlen(const char *str);

/*
* takes a string buffer and its length, reverses the string inside the buffer (in place).
*/
static void reverse(char *str, size_t len);

/*
 * takes a number, a character buffer (string) and a base (like 2, 10 and 16), and returns a string value of the number based on the base
 * it also puts the same number in the buffer it takes
 */
char *itoa(uint64_t num, char *buffer, int base);

/**
 * takes an unsigned long long (64 bit number), a character buffer and a base, and returns a string value of the number based on the base give.
 */
char *utoa(uint64_t num, char *buffer, int base);

#endif