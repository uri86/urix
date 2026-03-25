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

    uint64_t start_tsc = rdtsc();

    uint64_t end_tsc = rdtsc();

    if (cpu_freq_mhz == 0)
    {
        cpu_freq_mhz = 1000; // Assume 1 GHz if not calibrated
    }
}

/**
 * delay_ms - Execute a delay using RDTSC.
 *
 * ms: Duration of the delay in milliseconds.
 */
void delay_ms(uint32_t ms)
{
    if (!debug_mode)
        return;

    if (cpu_freq_mhz == 0)
    {
        cpu_freq_mhz = 1000;
    }

    uint64_t start = rdtsc();
    uint64_t ticks_to_wait = (uint64_t)ms * cpu_freq_mhz * 1000ULL;

    while ((rdtsc() - start) < ticks_to_wait)
    {
        __asm__ volatile("pause");
    }
}

void halt(void)
{
    while (1)
    {
        __asm__ volatile("hlt");
    }
}

uint64_t read_cr3(void)
{
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0": "=r"(cr3));
    return cr3;
}

void write_cr3(uint64_t value)
{
    __asm__ volatile ("mov %0, %%cr3" : : "r"(value) : "memory");
}