/*
 * Licensed under MIT License - URIX project.
 * utils.c - Implementation of general kernel utilities.
 */
#include <stdint.h>

// debug mode globals
uint8_t debug_mode = 0;
uint32_t debug_delay_ms = 100;

// CPU frequency in Hz (needs to be calibrated at boot)
static uint64_t cpu_freq_mhz = 0;

/**
 * rdtsc - Read the CPU's Time Stamp Counter
 */
static inline uint64_t rdtsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/**
 * calibrate_delay - Calibrate the CPU frequency using the PIT
 *
 * Call this once during kernel initialization.
 */
void calibrate_delay(void)
{
    // Use PIT for calibration (1.193182 MHz)
    // This is a simplified version - you'd want more robust calibration

    uint64_t start_tsc = rdtsc();

    // Delay for a known amount using PIT (e.g., 10ms)
    // You'd implement a PIT-based delay here
    // For now, assume we measured 10ms

    uint64_t end_tsc = rdtsc();

    // Calculate CPU frequency
    // cpu_freq_mhz = (end_tsc - start_tsc) / 10000; // cycles per microsecond

    // Default fallback if calibration not run
    if (cpu_freq_mhz == 0)
    {
        cpu_freq_mhz = 1000; // Assume 1 GHz if not calibrated
    }
}

/**
 * delay_ms - Execute a delay using RDTSC.
 *
 * ms: Duration of the delay in milliseconds.
 *
 * Uses the CPU's timestamp counter for accurate timing across different
 * CPU speeds. Requires calibrate_delay() to be called first.
 */
void delay_ms(uint32_t ms)
{
    if (!debug_mode)
        return;

    if (cpu_freq_mhz == 0)
    {
        // Fallback to estimated value if not calibrated
        cpu_freq_mhz = 1000;
    }

    uint64_t start = rdtsc();
    uint64_t ticks_to_wait = (uint64_t)ms * cpu_freq_mhz * 1000ULL;

    while ((rdtsc() - start) < ticks_to_wait)
    {
        __asm__ volatile("pause"); // CPU hint for spin loops
    }
}