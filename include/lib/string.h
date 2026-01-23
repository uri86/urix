/*
 * Licensed under MIT License - URIX project.
 * string.h - String and number conversion helpers for URIX.
 * Responsibilities:
 *  - declare strlen, reverse, itoa, utoa
 *  - provide lightweight replacement for libc string/stdio utilities
 * Notes:
 *  - focused on kernel use (no malloc, no locale support)
 *  - supports integer bases 2–36
 */

#ifndef STRING_H
#define STRING_H

#include <stdint.h>

/**
 * strlen - Return the length of a null-terminated string.
 *
 * str: Pointer to the string.
 *
 * Returns the number of characters before the first null byte.
 * Does not perform bounds checking—assumes valid and terminated input.
 */
uint64_t strlen(const char *str);

/**
 * strcpy - Copy a null-terminated string to a buffer.
 *
 * dest: Destination buffer.
 * src:  Source string.
 *
 * Returns:
 * Pointer to dest.
 * If either dest or src is NULL, returns NULL.
 */
char *strcpy(char *dest, const char *src);

/**
 * reverse - Reverse a string in place.
 *
 * str: Pointer to the string buffer.
 * len: Number of characters to reverse.
 *
 * Used primarily by numeric conversion functions.
 * Caller must ensure the buffer is writable and properly sized.
 */
void reverse(char *str, size_t len);

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
char *itoa(uint64_t num, char *buffer, int base);

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
char *utoa(uint64_t num, char *buffer, int base);

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
int strcmp(const char *s1, const char *s2);

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
int strncmp(const char *s1, const char *s2, size_t n);

/**
 * strncpy - Copy a fixed number of characters from one string to another.
 *
 * dest: Destination buffer.
 * src:  Source string.
 * n:    Maximum number of bytes to copy.
 *
 * Returns:
 *   Pointer to dest.
 *   Returns NULL if dest or src is NULL.
 *
 * Notes:
 *   - Does NOT guarantee null-termination if src length >= n.
 *   - Pads with '\0' bytes if src is shorter than n.
 *   - Caller must ensure dest has space for at least n bytes.
 */
char *strncpy(char *dest, const char *src, size_t n);

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
void *memset(void *dest, int value, uint64_t count);

/**
 * memcpy - Copy memory area.
 *
 * dest: Pointer to the destination memory area.
 * src:  Pointer to the source memory area.
 * n:    Number of bytes to copy.
 *
 * Returns a pointer to dest.
 * Note: Does NOT handle overlapping memory.
 */
void *memcpy(void *dest, const void *src, size_t n);

/**
 * memmove - Copy memory area.
 *
 * dest: Pointer to the destination memory area.
 * src:  Pointer to the source memory area.
 * n:    Number of bytes to copy.
 *
 * Returns a pointer to dest.
 * safely handles cases where src and dest overlap.
 */
void *memmove(void *dest, const void *src, size_t n);

/**
 * strcat - Concatenate two null-terminated strings.
 *
 * dest: Pointer to the destination buffer.
 * src:  Pointer to the source string to append.
 *
 * Appends the string pointed to by src to the end of the string
 * pointed to by dest. The resulting string in dest is always
 * null-terminated.
 *
 * Returns a pointer to dest.
 */

char *strcat(char *dest, const char *src);

#endif