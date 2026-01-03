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
 *
 * Note: This function never returns. It disables interrupts and halts the CPU.
 */
void panic(const char *message, const char *file, uint32_t line);

/*
 * PANIC Macro
 * Usage: PANIC("Out of memory");
 * Automatically passes the current file and line number.
 */
#define PANIC(msg) panic(msg, __FILE__, __LINE__)

#endif /* PANIC_H */