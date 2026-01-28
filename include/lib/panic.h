/*
 * Licensed under MIT License - URIX project.
 * panic.h - Kernel panic interface.
 * Responsibilities:
 * - Declare the panic function
 * - Provide a macro to automatically capture file and line number
 */

#ifndef PANIC_H
#define PANIC_H

#include <stdint.h>

/**
 * panic - Aborts kernel execution and prints a fatal error message.
 *
 * message: The error description.
 * file: The source file where the error occurred.
 * line: The line number in the source file.
 */
void panic(const char *message, const char *file, uint32_t line);

/*
 * PANIC Macro
 */
#define PANIC(msg) panic(msg, __FILE__, __LINE__)

#endif /* PANIC_H */