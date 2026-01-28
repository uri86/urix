/*
 * Licensed under MIT License - URIX project.
 * kmalloc.h - Kernel dynamic memory allocator interface.
 */

#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>

/*
 * kmalloc_init - Initialize the kernel heap allocator.
 *
 * This function sets up internal allocator structures and must be
 * called exactly once during early kernel initialization.
 *
 * Requirements:
 *  - Physical memory manager (PMM) initialized.
 *  - Virtual memory manager (VMM) initialized.
 */
void kmalloc_init(void);

/*
 * kmalloc - Allocate a block of dynamic memory from the kernel heap.
 *
 * Allocation strategy:
 *  - Requests <= 2048 bytes are served from slab allocators.
 *  - Requests > 2048 bytes are served by page-backed allocations.
 *
 * Behavior:
 *  - The returned pointer refers to usable memory only.
 *  - Internal metadata is stored before the returned pointer.
 *
 * Returns:
 *  - Pointer to allocated memory on success.
 *  - NULL if size == 0.
 *  - Kernel panic on out-of-memory conditions.
 */
void *kmalloc(size_t size);

/*
 * kfree - Free a block of memory previously allocated with kmalloc().
 * Parameters:
 *  - ptr: Pointer previously returned by kmalloc().
 *
 */
void kfree(void *ptr);

/*
 * kmalloc_print_stats - Print allocator usage statistics for debugging purposes.
 *
 * Output:
 *  - One line per slab size class.
 *  - Shows used blocks vs total blocks.
 */
void kmalloc_print_stats(void);

#endif /* KMALLOC_H */
