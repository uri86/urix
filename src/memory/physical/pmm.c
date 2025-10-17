/*
 * Licensed under MIT License - URIX project.
 * pmm.c - Physical Memory Manager with 4KB pages.
 * Responsibilities:
 *  - manage physical memory using a bitmap of 4KB frames
 *  - allocate and free individual physical frames
 *  - track total, free, and used memory
 *  - reserve memory for kernel, page tables, multiboot structures, and bitmap itself
 *  - build early identity mapping for low memory region
 *  - initialize memory from multiboot2 memory map
 *  - provide diagnostic printing of PMM state and statistics
 * Notes:
 *  - bitmap stores allocation state (1=used, 0=free) for each 4KB frame
 *  - ensures allocated frames are page-aligned
 *  - skips freeing frame 0
 *  - uses last-allocation optimization to speed up sequential allocations
 *  - marks non-usable memory and reserved regions as used
 *  - depends on identity_map.c for building early identity mapping
 */


#include <multiboot2.h>
#include <memory/physical/pmm.h>
#include <memory/physical/identity_map.h>
#include <lib/print.h>
#include <lib/string.h>
#include <stddef.h>
#include <stdint.h>

extern char _kernel_start;
extern char _kernel_end;

/* Bitmap state */
static uint8_t *bitmap = NULL;
static uint8_t bitmap_set = 0;
static uint64_t bitmap_size_bytes = 0;
static uint64_t bitmap_num_frames = 0;

/* Statistics */
static uint64_t total_frames = 0;
static uint64_t free_frames = 0;
static uint64_t highest_usable_addr = 0;

/* Optimization: last allocation position */
static uint64_t last_alloc_byte = 0;

/* Utility functions */
static inline uint64_t align_up(uint64_t x, uint64_t align) { return (x + align - 1) & ~(align - 1); }
static inline uint64_t align_down(uint64_t x, uint64_t align) { return x & ~(align - 1); }
static inline uint64_t div_round_up(uint64_t x, uint64_t divisor) { return (x + divisor - 1) / divisor; }

/* Bitmap operations */
static inline int test_frame(uint64_t frame_idx)
{
    if (!bitmap || frame_idx >= bitmap_num_frames)
        return 1;

    uint64_t byte = frame_idx >> 3;
    uint8_t bit = 1U << (frame_idx & 7);
    return (bitmap[byte] & bit) != 0;
}

static inline void set_frame(uint64_t frame_idx)
{
    if (!bitmap || frame_idx >= bitmap_num_frames)
        return;

    uint64_t byte = frame_idx >> 3;
    uint8_t bit = 1U << (frame_idx & 7);

    if (!(bitmap[byte] & bit))
    {
        bitmap[byte] |= bit;
        if (free_frames > 0)
            free_frames--;
    }
}

static inline void clear_frame(uint64_t frame_idx)
{
    if (!bitmap_set || frame_idx >= bitmap_num_frames)
        return;

    uint64_t byte = frame_idx >> 3;
    uint8_t bit = 1U << (frame_idx & 7);

    if (bitmap[byte] & bit)
    {
        bitmap[byte] &= ~bit;
        free_frames++;
    }
}

/* Mark a physical range as used */
static void mark_region_used(uint64_t phys_start, uint64_t phys_end)
{
    kprintf("Marking region: [%llx - %llx]\n", phys_start, phys_end);
    if (phys_end <= phys_start)
        return;

    uint64_t frame_start = phys_start / PAGE_SIZE;
    uint64_t frame_end = div_round_up(phys_end, PAGE_SIZE);

    if (frame_start >= bitmap_num_frames)
        return;

    if (frame_end > bitmap_num_frames)
        frame_end = bitmap_num_frames;

    for (uint64_t i = frame_start; i < frame_end; i++)
        set_frame(i);
}

/* Initialize bitmap */
static void init_bitmap(uint64_t bitmap_phys, uint64_t size_bytes, uint64_t num_frames)
{
    bitmap = (uint8_t *)(uintptr_t)bitmap_phys;
    bitmap_size_bytes = size_bytes;
    bitmap_num_frames = num_frames;
    bitmap_set = 1;

    kprintf("init_bitmap: base=%llx size=%llu bytes (%llu frames)\n",
            bitmap_phys, size_bytes, num_frames);

    /* Zero bitmap - all frames start free */
    // memset(bitmap, 0, bitmap_size_bytes);
    free_frames = bitmap_num_frames;
}

uint64_t pmm_alloc_frame(void)
{
    if (!bitmap_set)
    {
        kprintf("pmm_alloc_frame: ERROR - PMM not initialized\n");
        return 0;
    }

    if (free_frames == 0)
    {
        kprintf("pmm_alloc_frame: ERROR - out of memory\n");
        return 0;
    }

    /* Search from last allocation point */
    uint64_t start_byte = last_alloc_byte;

    for (uint64_t offset = 0; offset < bitmap_size_bytes; offset++)
    {
        uint64_t byte_idx = (start_byte + offset) % bitmap_size_bytes;

        if (bitmap[byte_idx] != 0xFF)
        {
            for (int bit = 0; bit < 8; bit++)
            {
                uint8_t mask = 1U << bit;

                if (!(bitmap[byte_idx] & mask))
                {
                    /* Found free frame */
                    bitmap[byte_idx] |= mask;
                    last_alloc_byte = byte_idx;

                    uint64_t frame_idx = (byte_idx << 3) + bit;

                    if (frame_idx >= bitmap_num_frames)
                        return 0;

                    free_frames--;
                    return frame_idx * PAGE_SIZE;
                }
            }
        }
    }

    kprintf("pmm_alloc_frame: ERROR - no free frames (inconsistent state)\n");
    return 0;
}

void pmm_free_frame(uint64_t phys_addr)
{
    if (!bitmap_set)
        return;

    if (phys_addr % PAGE_SIZE)
    {
        kprintf("pmm_free_frame: ERROR - address %llx not page-aligned\n", phys_addr);
        return;
    }

    uint64_t frame_idx = phys_addr / PAGE_SIZE;

    if (frame_idx >= bitmap_num_frames)
    {
        kprintf("pmm_free_frame: ERROR - frame %llu out of range\n", frame_idx);
        return;
    }

    /* Don't free frame 0 */
    if (frame_idx == 0)
        return;

    clear_frame(frame_idx);
}

uint64_t pmm_get_free_frames(void) { return free_frames; }
uint64_t pmm_get_total_frames(void) { return total_frames; }

void pmm_print_stats(void)
{
    kprintf("\n=== PMM Statistics ===\n");
    kprintf("Total memory: %llu MB (%llu frames)\n", 
            (total_frames * PAGE_SIZE) / (1024 * 1024), total_frames);
    kprintf("Free: %llu MB (%llu frames)\n",
            (free_frames * PAGE_SIZE) / (1024 * 1024), free_frames);
    kprintf("Used: %llu MB (%llu frames)\n",
            ((total_frames - free_frames) * PAGE_SIZE) / (1024 * 1024),
            total_frames - free_frames);
    kprintf("Highest usable: %llx (%llu MiB)\n",
            highest_usable_addr, highest_usable_addr / (1024 * 1024));
    kprintf("Bitmap: %llu KB\n", bitmap_size_bytes / 1024);
    kprintf("======================\n\n");
}

void pmm_init(multiboot_size_tag *s)
{
    uint64_t reserved_multiboot_range = align_up((uint64_t)s + (uint64_t)s->total_size, PAGE_SIZE);
    kprintf("\n=== Initializing PMM ===\n");

    uint64_t usable_bytes = 0;
    multiboot_tag *tag = (multiboot_tag *)((uint8_t *)s + 8);
    multiboot_tag_mmap *mmap_tag = NULL;

    /* Find memory map and calculate totals */
    while (tag->type != MULTIBOOT_TAG_TYPE_END)
    {
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP)
        {
            mmap_tag = (multiboot_tag_mmap *)tag;
            uint32_t entry_count = (mmap_tag->size - sizeof(*mmap_tag)) / mmap_tag->entry_size;

            kprintf("Memory map (%u entries):\n", entry_count);

            for (uint32_t i = 0; i < entry_count; i++)
            {
                multiboot_mmap_entry *entry = &mmap_tag->entries[i];

                kprintf("  [%llx - %llx] type=%u (%llu KB)\n",
                        entry->addr, entry->addr + entry->len,
                        entry->type, entry->len / 1024);

                if (entry->type == MULTIBOOT_MMAP_AVAILABLE)
                {
                    uint64_t start = align_up(entry->addr, PAGE_SIZE);
                    uint64_t end = align_down(entry->addr + entry->len, PAGE_SIZE);

                    if (end > start)
                    {
                        uint64_t frames = (end - start) / PAGE_SIZE;
                        total_frames += frames;
                        usable_bytes += (end - start);
                        if (end > highest_usable_addr)
                            highest_usable_addr = end;
                    }
                }
            }
        }

        tag = (multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7));
    }

    if (!mmap_tag)
    {
        kprintf("FATAL: No memory map found\n");
        return;
    }

    kprintf("\nTotal usable RAM: %llu MB (%llu frames)\n",
            usable_bytes / (1024 * 1024), total_frames);
    kprintf("Highest usable address: %llx (%llu MiB)\n",
            highest_usable_addr, highest_usable_addr / (1024 * 1024));

    /* Calculate bitmap size */
    uint64_t addr_space_frames = div_round_up(highest_usable_addr, PAGE_SIZE);
    uint64_t bitmap_bytes_needed = div_round_up(addr_space_frames, 8);

    kprintf("Bitmap size: %llu KB for %llu frames\n",
            bitmap_bytes_needed / 1024, addr_space_frames);

    /* Get kernel boundaries */
    uint64_t kernel_start = (uint64_t)&_kernel_start;
    uint64_t kernel_end = align_up((uint64_t)&_kernel_end, PAGE_SIZE);

    kprintf("Kernel: [%llx - %llx] (%llu KB)\n",
            kernel_start, kernel_end, (kernel_end - kernel_start) / 1024);

    /* Reserve PT allocation area */
    if (kernel_end >= align_down((uint64_t)s, PAGE_SIZE))
        kernel_end = reserved_multiboot_range;
    kprintf("Kernel end: %llx, Multiboot start: %llx\n", kernel_end, align_down((uint64_t)s, PAGE_SIZE));
    
    uint64_t pt_alloc_start = align_up(kernel_end, PAGE_SIZE);
    uint64_t pt_alloc_end = pt_alloc_start + PT_RESERVE_BYTES;

    if (pt_alloc_end > EARLY_IDENTITY_LIMIT)
    {
        kprintf("FATAL: PT area exceeds early identity map.\n");
        return;
    }

    kprintf("PT reserve: [%llx - %llx] (%llu MB)\n",
            pt_alloc_start, pt_alloc_end, PT_RESERVE_BYTES / (1024 * 1024));

    /* Find space for bitmap */
    uint64_t bitmap_start = 0;
    uint64_t bitmap_end = 0;
    int found = 0;

    tag = (multiboot_tag *)((uint8_t *)s + 8);
    while (tag->type != MULTIBOOT_TAG_TYPE_END)
    {
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP)
        {
            multiboot_tag_mmap *mm = (multiboot_tag_mmap *)tag;
            uint32_t count = (mm->size - sizeof(*mm)) / mm->entry_size;

            for (uint32_t i = 0; i < count; i++)
            {
                multiboot_mmap_entry *entry = &mm->entries[i];

                if (entry->type != MULTIBOOT_MMAP_AVAILABLE)
                    continue;

                uint64_t region_start = align_up(entry->addr, PAGE_SIZE);
                uint64_t region_end = align_down(entry->addr + entry->len, PAGE_SIZE);

                /* Skip kernel */
                if (region_start < kernel_end && region_end > kernel_start)
                    region_start = kernel_end;

                /* Skip PT area */
                if (region_start < pt_alloc_end && region_end > pt_alloc_start)
                    region_start = pt_alloc_end;

                if (region_end <= region_start)
                    continue;

                if (region_end - region_start >= bitmap_bytes_needed)
                {
                    bitmap_start = region_start;
                    bitmap_end = region_start + bitmap_bytes_needed;
                    found = 1;
                    kprintf("Bitmap: [%llx - %llx]\n", bitmap_start, bitmap_end);
                    break;
                }
            }
            if (found) break;
        }

        tag = (multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7));
    }

    if (!found)
    {
        kprintf("FATAL: No space for bitmap\n");
        return;
    }

    /* Build identity map */
    kprintf("\nBuilding identity map...\n");
    uint64_t map_end = align_up(highest_usable_addr, PAGE_SIZE);

    if (identity_map_all(map_end, pt_alloc_start, pt_alloc_end) != 0)
    {
        kprintf("FATAL: Failed to build identity mapping\n");
        return;
    }

    /* Initialize bitmap */
    init_bitmap(bitmap_start, bitmap_bytes_needed, addr_space_frames);

    /* Mark reserved regions */
    kprintf("\nMarking reserved regions...\n");
    mark_region_used(0, PAGE_SIZE);  /* Frame 0 */
    mark_region_used(kernel_start, kernel_end);
    mark_region_used(align_down((uint64_t)s, PAGE_SIZE), align_up((uint64_t)reserved_multiboot_range, PAGE_SIZE));
    mark_region_used(pt_alloc_start, pt_alloc_end);
    mark_region_used(bitmap_start, bitmap_end);

    /* Mark non-usable memory */
    tag = (multiboot_tag *)((uint8_t *)s + 8);
    while (tag->type != MULTIBOOT_TAG_TYPE_END)
    {
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP)
        {
            multiboot_tag_mmap *mm = (multiboot_tag_mmap *)tag;
            uint32_t count = (mm->size - sizeof(*mm)) / mm->entry_size;

            for (uint32_t i = 0; i < count; i++)
            {
                multiboot_mmap_entry *entry = &mm->entries[i];
                if (entry->type != MULTIBOOT_MMAP_AVAILABLE)
                {
                    uint64_t start = align_down(entry->addr, PAGE_SIZE);
                    uint64_t end = align_up(entry->addr + entry->len, PAGE_SIZE);
                    mark_region_used(start, end);
                }
            }
        }
        tag = (multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7));
    }

    kprintf("\n=== PMM Initialization Complete ===\n");
    pmm_print_stats();
}