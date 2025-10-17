/*
 * Licensed under MIT License - URIX project.
 * identity_map.h - Identity Mapping interface for x86-64 with 4KB pages.
 * Responsibilities:
 *  - define constants and flags for page table entries
 *  - declare functions for page table allocation
 *  - provide function to build identity mapping for a physical memory range
 *  - expose diagnostic function to print page table allocator usage
 * Notes:
 *  - uses 4-level page tables (PML4, PDPT, PD, PT)
 *  - PAGE_PRESENT_RW combines present and writable flags
 *  - identity_map_all maps virtual addresses equal to physical addresses
 *  - allocator functions may be called externally if needed
 */

#ifndef IDENTITY_MAP_H
#define IDENTITY_MAP_H

#include <stdint.h>
#include <stddef.h>

#define PTE_ENTRIES 512U

/* Page table entry flags */
#define PAGE_PRESENT 0x1ULL
#define PAGE_WRITE 0x2ULL
#define PAGE_USER 0x4ULL
#define PAGE_PRESENT_RW (PAGE_PRESENT | PAGE_WRITE)

/* Initialize the page table allocator (if needed externally) */
void pt_alloc_init(uint64_t start_phys, uint64_t limit_phys);

/* Allocate one page for page tables (returns physical address or 0 on fail) */
uint64_t pt_alloc_page_phys(void);

/* Build identity mapping using 4KB pages.
 * map_end: inclusive end (address range [0 .. map_end) will be mapped).
 * pt_alloc_start / pt_alloc_limit: physical range used to allocate page-table pages
 *
 * Returns 0 on success, -1 on error.
 */
int identity_map_all(uint64_t map_end, uint64_t pt_alloc_start, uint64_t pt_alloc_limit);

/* Print statistics of page-table allocator usage */
void pt_alloc_print_usage(void);

#endif /* IDENTITY_MAP_H */
