/*
 * Licensed under MIT License - URIX project.
 * utils.c - Implementation of general kernel utilities.
 * Responsibilities:
 * - initialize global debug state variables (debug_mode, debug_delay_ms)
 * - implement the delay_ms busy-wait function
 * Notes:
 * - The busy-wait loop uses inline assembly ("nop") for timing.
 * - This file requires standard integer types from <stdint.h>.
 */

#include <stdint.h>

// debug mode globals
/**
 * debug_mode - Global flag to enable or disable debug features.
 *
 * Value:
 * 0  -> Debug features are disabled (minimal overhead).
 * >0 -> Debug features (like delay_ms) are enabled.
 */
short debug_mode = 0;

/**
 * debug_delay_ms - Default delay duration for debug functions.
 *
 * Controls the duration (in milliseconds) of certain debug-related delays.
 * Default value is 100ms.
 */
uint32_t debug_delay_ms = 100;

/**
 * delay_ms - Execute a busy-wait delay if debug mode is active.
 *
 * ms: Duration of the delay in milliseconds.
 *
 * Performs a simple, loop-based busy-wait. This function is **non-preemptible**
 * and should only be used in early kernel stages or for debugging.
 *
 * The function is a NOP (No-Operation) if debug_mode is 0.
 * Calculation is based on an estimated cycle count: (ms * 1,000,000ULL).
 */
void delay_ms(uint32_t ms)
{
    if (!debug_mode)
        return;
    // Calculate estimated cycle count for busy-wait.
    // 1,000,000 operations per ms is a rough estimate for 1ms on a 1GHz CPU,
    // adjusted for compiler optimization impact on the loop.
    volatile uint64_t count = (uint64_t)ms * 1000000ULL;

    for (volatile uint64_t i = 0; i < count; i++)
    {
        // Prevent the compiler from optimizing the loop away entirely.
        __asm__ volatile("nop");
    }
}