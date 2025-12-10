/*
 * Licensed under MIT License - URIX project.
 * utils.h - General kernel utilities, focused on debugging and simple timing.
 * Responsibilities:
 * - declare debug_mode and debug_delay_ms globals
 * - declare delay_ms function for busy-wait timing
 * Notes:
 * - Provides simple, non-scheduler-dependent timing for early kernel phases.
 * - delay_ms is conditional on debug_mode.
 */

#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

/**
 * debug_mode - Global flag to enable or disable debug features.
 *
 * Value:
 * 0  -> Debug features are disabled (minimal overhead).
 * >0 -> Debug features (like delay_ms) are enabled.
 *
 * Declared as 'extern' here; defined and initialized in utils.c.
 */
extern short debug_mode;

/**
 * debug_delay_ms - Default delay duration for debug functions.
 *
 * Controls the duration (in milliseconds) of certain debug-related delays.
 * Default value is 100ms.
 * Declared as 'extern' here; defined and initialized in utils.c.
 */
extern uint32_t debug_delay_ms;

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
void delay_ms(uint32_t ms);

#endif /* UTILS_H */