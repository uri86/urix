/*
 * Licensed under MIT License - URIX project.
 * stdio.h - Formatted string operations for shared libc.
 */

#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>
#include <stdarg.h>

/**
 * vsnprintf - Format strings into a buffer securely
 */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);

/**
 * snprintf - printf targeting a bounded buffer natively
 */
int snprintf(char *buf, size_t size, const char *fmt, ...);

#endif /* STDIO_H */
