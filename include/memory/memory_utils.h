/*
 * Licensed under MIT License - URIX project.
 * memory_utils.h - Shared memory utilities and macros.
 * Responsibilities:
 *  - provide alignment calculation helpers
 *  - provide page table index extraction macros
 *  - provide physical extraction from page table entries
 */

#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

#include <stdint.h>

/* Alignment utilities */
static inline uint64_t align_up(uint64_t x, uint64_t a) { return (x + a - 1) & ~(a - 1); }
static inline uint64_t align_down(uint64_t x, uint64_t a) { return x & ~(a - 1); }
static inline uint64_t div_round_up(uint64_t x, uint64_t d) { return (x + d - 1) / d; }

/*
 * Extract page table indices from virtual address
 *
 * x86-64 virtual address (48-bit):
 *   Bits 39-47: PML4 index (9 bits)
 *   Bits 30-38: PDPT index (9 bits)
 *   Bits 21-29: PD index   (9 bits)
 *   Bits 12-20: PT index   (9 bits)
 *   Bits 0-11:  Page offset (12 bits)
 */
static inline uint64_t pml4_index(uint64_t vaddr) { return (vaddr >> 39) & 0x1FF; }
static inline uint64_t pdpt_index(uint64_t vaddr) { return (vaddr >> 30) & 0x1FF; }
static inline uint64_t pd_index(uint64_t vaddr) { return (vaddr >> 21) & 0x1FF; }
static inline uint64_t pt_index(uint64_t vaddr) { return (vaddr >> 12) & 0x1FF; }

/*
 * Extract physical address from page table entry
 * Clears all flag bits to get just the address
 */
static inline uint64_t pte_to_phys(uint64_t pte)
{
    return pte & 0x000FFFFFFFFFF000ULL;
}

#endif /* MEMORY_UTILS_H */
