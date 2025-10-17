/*
 * Licensed under MIT License - URIX project.
 * pmm.h - Physical Memory Manager interface with 4KB pages.
 * Responsibilities:
 *  - declare PMM initialization and management functions
 *  - provide allocation and freeing of 4KB physical frames
 *  - expose functions to query total and free frames
 *  - reserve space for page tables and early identity mapping
 *  - provide diagnostic function to print PMM statistics
 * Notes:
 *  - PAGE_SIZE is fixed at 4KB
 *  - EARLY_IDENTITY_LIMIT defines the maximum address for early identity mapping
 *  - PT_RESERVE_BYTES reserves memory for page tables during early boot
 *  - pmm_init uses multiboot2 memory map to set up usable memory regions
 */

#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include <multiboot2.h>

#define PAGE_SIZE 4096ULL

/* Early identity map from bootloader - typically 1 GiB */
#define EARLY_IDENTITY_LIMIT (1ULL << 30)

/* Reserve space for page tables with 4KB pages
 * 64 MiB default reserve (tunable)
 */
#define PT_RESERVE_BYTES (64ULL * 1024 * 1024)

/* Initialize the physical memory manager */
void pmm_init(multiboot_size_tag *s);

/* Allocate a single physical frame (4 KiB)
 * Returns physical address (non-zero) or 0 on failure.
 */
uint64_t pmm_alloc_frame(void);

/* Free a physical frame */
void pmm_free_frame(uint64_t phys_addr);

/* Get number of free frames */
uint64_t pmm_get_free_frames(void);

/* Get total number of frames */
uint64_t pmm_get_total_frames(void);

/* Print PMM statistics */
void pmm_print_stats(void);

#endif /* PMM_H */
