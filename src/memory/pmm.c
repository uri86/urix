/*
 * Licensed under MIT License - URIX project.
 * pmm.c - Physical Memory Manager initialization, helpers and allocation.
 * Responsibilities:
 *  - provide alignment helpers (align_up, align_down)
 *  - convert raw memory sizes to MB for reporting
 *  - parse multiboot2 memory map and calculate total physical memory
 *  - count and prepare frame information for future allocation
 * Notes:
 *  - relies on multiboot2 memory map tags (MULTIBOOT_TAG_TYPE_MMAP)
 *  - page alignment is enforced based on PAGE_SIZE
 *  - outputs system memory summary for debugging
 */

#include <memory/pmm.h>
#include <lib/print.h>

/* Round `x` up to the next multiple of `align` (align must be power-of-two) */
static inline uint64_t align_up(uint64_t x, uint64_t align)
{
    return (x + (align - 1)) & ~(align - 1);
}

/* Round `x` down to the previous multiple of `align` (align power-of-two) */
static inline uint64_t align_down(uint64_t x, uint64_t align)
{
    return x & ~(align - 1);
}

/* Convert bytes -> MB (binary MB = 1024*1024 bytes) */
static inline uint64_t bytes_to_mb(uint64_t bytes)
{
    return bytes / (1024ULL * 1024ULL);
}

void pmm_init(multiboot_size_tag *s)
{
    uint64_t mem_size = 0;
    // uint16_t size = 0;
    uint64_t total_frames = 0;
    multiboot_tag *tag = (multiboot_tag *)(uintptr_t)(s + 8);
    while(tag->type != MULTIBOOT_TAG_TYPE_END)
    {
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP)
        {
            multiboot_tag_mmap *mmap = (multiboot_tag_mmap *)tag;
            uint32_t entry_count = (mmap->size - sizeof(*mmap)) / mmap->entry_size;
            for (uint32_t i = 0; i < entry_count; i++)
            {
                multiboot_mmap_entry *e = &mmap->entries[i];
                mem_size += e->len;
                if (e->type == MULTIBOOT_MMAP_AVAILABLE)
                {
                    uint64_t start = align_up(e->addr, PAGE_SIZE);
                    uint64_t end = align_down(e->addr + e->len, PAGE_SIZE);
                    if (end > start)
                    {
                        uint64_t bytes_in_region = end - start;
                        uint64_t frames = bytes_in_region / PAGE_SIZE;
                        total_frames += frames;
                    }
                }
            }
        }
        tag = (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7));
    }
    // if (total_frames % 8 != 0)
    //     size = total_frames/8 + 1;
    // else
    //     size = total_frames/8;
    
    //int8_t frames[size]; // bitmap of the frames in the system?

    kprintf("Summery:\n");
    kprintf("Multiboot Header size: %llx\n", s->total_size);
    kprintf("Total Frames: %llu\n", total_frames);
    kprintf("Total memory: %llu\n", bytes_to_mb(mem_size));

}