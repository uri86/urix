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

// The identity map established by the bootloader/early kernel.
#define EARLY_IDENTITY_LIMIT (1ULL << 30)

#define EARLY_MAP_LIMIT (1ULL << 30)

// Maximum amount of RAM the kernel should track, set to 128 GiB.
#define MAX_PHYS_ADDR (128ULL * 1024 * 1024 * 1024)

#define MAX_BITMAP_BYTES (MAX_PHYS_ADDR / PAGE_SIZE / 8)

// Reserve space for page tables that the kernel needs to set up *before* it can use the VMM.
// 8 MiB is a safe, conservative size that should fit in the first memory region.
#define PT_RESERVE_BYTES (8ULL * 1024 * 1024)

#define MAX_MEM_REGIONS 128
typedef struct mem_region
{
    uint64_t start;
    uint64_t end;
    uint32_t type; // part of the multiboot memory entry types.
} mem_region;

/* Initialize the physical memory manager */
void pmm_init(multiboot_size_tag *s);

/* Allocate a single 4KB physical frame and return its address */
uint64_t pmm_alloc_frame(void);

/* Free a physical frame by marking it as available */
void pmm_free_frame(uint64_t phys_addr);

uint64_t pmm_get_free_frames(void);
uint64_t pmm_get_total_frames(void);

void pmm_print_stats(void);

#endif /* PMM_H */