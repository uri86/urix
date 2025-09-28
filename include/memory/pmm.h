/*
 * pmm.h - header file for the physical memory manager.
 * Responsibilities:
 *  - define multiboot struct types
 *  - include definition of different functions.
 */
#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <multiboot2.h>

/* PAGE_SIZE is 4 KiB (we'll count frames of this size) */
#define PAGE_SIZE 4096ULL

void pmm_init(multiboot_size_tag *s);

void pmm_alloc_frame();

void pmm_free_frame();

#endif