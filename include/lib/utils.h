/*
 * Licensed under MIT License - URIX project.
 * utils.h - General kernel utilities, focused on debugging and simple timing.
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
 */
extern uint8_t debug_mode;

/**
 * debug_delay_ms - Default delay duration for debug functions.
 *
 * Controls the duration (in milliseconds) of certain debug-related delays.
 */
extern uint32_t debug_delay_ms;

/**
 * delay_ms - Execute a busy-wait delay if debug mode is active.
 *
 * ms: Duration of the delay in milliseconds.
 */
void delay_ms(uint32_t ms);

#endif /* UTILS_H */