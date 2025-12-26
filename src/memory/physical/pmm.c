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
#include <memory/vmm.h>
#include <lib/print.h>
#include <lib/string.h>
#include <stddef.h>
#include <stdint.h>

/* linker-provided kernel boundaries */
extern char _kernel_start;
extern char _kernel_end;

/* Internal state */
static uint8_t *bitmap = NULL;
static uint8_t bitmap_set = 0;
static uint64_t bitmap_size_bytes = 0;
static uint64_t bitmap_num_frames = 0;

/* stats */
static uint64_t total_frames = 0;
static uint64_t free_frames = 0;
static uint64_t highest_usable_addr = 0;
static uint64_t last_alloc_byte = 0;

/* PT reserve */
static uint64_t pt_reserve_start = 0;
static uint64_t pt_reserve_end = 0;

/* regions from multiboot2 header*/
mem_region regions[MAX_MEM_REGIONS];
uint32_t regions_count = 0;

/* small helpers */
static inline uint64_t align_up(uint64_t x, uint64_t a) { return (x + a - 1) & ~(a - 1); }
static inline uint64_t align_down(uint64_t x, uint64_t a) { return x & ~(a - 1); }
static inline uint64_t div_round_up(uint64_t x, uint64_t d) { return (x + d - 1) / d; }

static inline int test_frame_internal(uint64_t frame_idx)
{
    if (!bitmap || frame_idx >= bitmap_num_frames)
        return 1;
    uint64_t byte_idx = frame_idx >> 3;
    uint8_t bit = 1U << (frame_idx & 7);
    return (bitmap[byte_idx] & bit) != 0;
}

static inline void set_frame_internal(uint64_t frame_idx)
{
    if (!bitmap || frame_idx >= bitmap_num_frames)
        return;
    uint64_t byte_idx = frame_idx >> 3;
    uint8_t bit = 1U << (frame_idx & 7);
    if (!(bitmap[byte_idx] & bit))
    {
        bitmap[byte_idx] |= bit;
        if (free_frames > 0)
            free_frames--;
    }
}

static inline void clear_frame_internal(uint64_t frame_idx)
{
    if (!bitmap_set || frame_idx >= bitmap_num_frames)
        return;
    uint64_t byte_idx = frame_idx >> 3;
    uint8_t bit = 1U << (frame_idx & 7);
    if (bitmap[byte_idx] & bit)
    {
        bitmap[byte_idx] &= ~bit;
        free_frames++;
    }
}

static void mark_region_used_internal(uint64_t phys_start, uint64_t phys_end)
{
    if (phys_end <= phys_start)
        return;
    kprintf("Marking region used: [%llx - %llx]\n", (uint64_t)phys_start, (uint64_t)phys_end);

    uint64_t frame_start = phys_start / PAGE_SIZE;
    uint64_t frame_end = div_round_up(phys_end, PAGE_SIZE);
    if (frame_start >= bitmap_num_frames)
        return;
    if (frame_end > bitmap_num_frames)
        frame_end = bitmap_num_frames;

    for (uint64_t i = frame_start; i < frame_end; ++i)
        set_frame_internal(i);
}

/* Allocate a free 4KiB frame. Returns physical address (page-aligned) or 0 on failure. */
uint64_t pmm_alloc_frame(void)
{
    if (!bitmap_set)
    {
        kprintf("pmm_alloc_frame: ERROR - PMM not initialized (bitmap_set=0)\n");
        return 0;
    }

    if (free_frames == 0)
    {
        kprintf("pmm_alloc_frame: ERROR - out of memory (free_frames==0)\n");
        pmm_print_stats();
        return 0;
    }

    uint64_t start = last_alloc_byte;
    for (uint64_t offset = 0; offset < bitmap_size_bytes; ++offset)
    {
        uint64_t byte_idx = (start + offset) % bitmap_size_bytes;

        if (bitmap[byte_idx] != 0xFF)
        {
            for (int bit = 0; bit < 8; ++bit)
            {
                uint8_t mask = 1U << bit;
                if (!(bitmap[byte_idx] & mask))
                {
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

    kprintf("pmm_alloc_frame: ERROR - inconsistent bitmap state (no free bit found)\n");
    pmm_print_stats();
    return 0;
}

/* Free a physical frame (phys_addr must be page aligned). Frame 0 is never freed. */
void pmm_free_frame(uint64_t phys_addr)
{
    if (!bitmap_set)
        return;

    if (phys_addr % PAGE_SIZE)
    {
        kprintf("pmm_free_frame: ERROR - address %llx not page-aligned\n", (uint64_t)phys_addr);
        return;
    }

    uint64_t frame_idx = phys_addr / PAGE_SIZE;
    if (frame_idx >= bitmap_num_frames)
    {
        kprintf("pmm_free_frame: ERROR - frame %llu out of range\n", (uint64_t)frame_idx);
        return;
    }

    if (frame_idx == 0)
        return; /* never free frame 0 */
    clear_frame_internal(frame_idx);
}

/* Accessors */
uint64_t pmm_get_free_frames(void) { return free_frames; }
uint64_t pmm_get_total_frames(void) { return total_frames; }

/* Print PMM stats */
void pmm_print_stats(void)
{
    kprintf("\n=== PMM Statistics ===\n");
    kprintf("Total memory: %llu MB (%llu frames)\n",
            (uint64_t)((total_frames * PAGE_SIZE) / (1024 * 1024)), (uint64_t)total_frames);
    kprintf("Free: %llu MB (%llu frames)\n",
            (uint64_t)((free_frames * PAGE_SIZE) / (1024 * 1024)), (uint64_t)free_frames);
    kprintf("Used: %llu MB (%llu frames)\n",
            (uint64_t)(((total_frames - free_frames) * PAGE_SIZE) / (1024 * 1024)),
            (uint64_t)(total_frames - free_frames));
    kprintf("Highest usable: %llx (%llu MiB)\n",
            (uint64_t)highest_usable_addr, (uint64_t)(highest_usable_addr / (1024 * 1024)));
    kprintf("Bitmap: %llu KB at phys %llx\n",
            (uint64_t)(bitmap_size_bytes / 1024),
            (uint64_t)((uintptr_t)bitmap));
    kprintf("PT reserve: [%llx - %llx]\n", (uint64_t)pt_reserve_start, (uint64_t)pt_reserve_end);
    kprintf("======================\n\n");
}
/**
 * Helper function to check if two ranges overlap
 */
static inline int ranges_overlap(uint64_t start1, uint64_t end1,
                                 uint64_t start2, uint64_t end2)
{
    return (start1 < end2) && (start2 < end1);
}

static uint64_t find_bitmap_region(
    uint64_t search_limit_addr,
    uint64_t bitmap_bytes_needed,
    uint64_t kernel_start,
    uint64_t kernel_end,
    uint64_t pt_start,
    uint64_t pt_end,
    uint64_t multiboot_start,
    uint64_t multiboot_end)
{
    const uint64_t first_mg = 0x100000;

    for (uint32_t i = 0; i < regions_count; i++)
    {
        uint64_t region_start = regions[i].start;
        uint64_t region_end = regions[i].end;

        debug_kprintf("Searching for bitmap in region: [%llx - %llx]\n", region_start, region_end);

        if (regions[i].type != MULTIBOOT_MMAP_AVAILABLE)
            continue;

        // Calculate the effective start and end for the search
        uint64_t aligned_start = (region_start > first_mg) ? region_start : first_mg;
        aligned_start = (aligned_start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        uint64_t region_search_end = (region_end < search_limit_addr) ? region_end : search_limit_addr;

        // Skip if the region is too small after capping and alignment
        if (aligned_start >= region_search_end || (aligned_start + bitmap_bytes_needed > region_search_end))
        {
            debug_kprintf("  -> Region too small or capped by limit. Skipping.\n");
            continue;
        }

        uint64_t candidate = aligned_start;

        while (candidate + bitmap_bytes_needed <= region_search_end)
        {
            uint64_t candidate_end = candidate + bitmap_bytes_needed;

            debug_kprintf("  Trying: [%llx - %llx]\n", candidate, candidate_end);

            // Check if overlaps with kernel
            if (ranges_overlap(candidate, candidate_end, kernel_start, kernel_end))
            {
                debug_kprintf("  -> Overlaps with kernel, jumping\n");
                candidate = (kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
                continue;
            }

            // Check if overlaps with page tables
            if (ranges_overlap(candidate, candidate_end, pt_start, pt_end))
            {
                debug_kprintf("  -> Overlaps with page tables, jumping\n");
                candidate = (pt_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
                continue;
            }

            // Check if overlaps with multiboot structures
            if (ranges_overlap(candidate, candidate_end, multiboot_start, multiboot_end))
            {
                debug_kprintf("  -> Overlaps with multiboot, jumping\n");
                candidate = (multiboot_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
                continue;
            }

            // Found a suitable region!
            debug_kprintf("  -> FOUND suitable region at %llx\n", candidate);
            return candidate;

            // Advance to the next page for the next check (guaranteed advancement)
            candidate += PAGE_SIZE;
        }

        debug_kprintf("  -> No suitable space found in region [%llx - %llx]\n", region_start, region_end);
    }

    debug_kprintf("  -> No suitable space found in any available region\n");
    return 0;
}

void pmm_init(multiboot_size_tag *s)
{
    if (!s)
    {
        kprintf("pmm_init: FATAL - null multiboot pointer\n");
        return;
    }

    kprintf("\n=== Initializing PMM ===\n");

    /* Calculate multiboot region boundaries */
    uint64_t multiboot_start = align_down((uint64_t)s, PAGE_SIZE);
    uint64_t multiboot_end = align_up((uint64_t)s + s->total_size, PAGE_SIZE);
    debug_kprintf("Multiboot info: [%llx - %llx] (%llu KB)\n",
                  multiboot_start, multiboot_end,
                  (multiboot_end - multiboot_start) / 1024);

    /* Scan memory map for totals */
    multiboot_tag *tag = (multiboot_tag *)((uint8_t *)s + 8);
    multiboot_tag_mmap *mmap_tag = NULL;
    total_frames = 0;
    highest_usable_addr = 0;
    uint64_t usable_bytes = 0;

    // Reset global counter before scan
    regions_count = 0;

    while (tag->type != MULTIBOOT_TAG_TYPE_END)
    {
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP)
        {
            mmap_tag = (multiboot_tag_mmap *)tag;
            uint32_t entry_count = (mmap_tag->size - sizeof(*mmap_tag)) / mmap_tag->entry_size;
            kprintf("Memory map (%u entries):\n", entry_count);

            // CORRECT: Use byte-level pointer arithmetic with entry_size
            multiboot_mmap_entry *entry = mmap_tag->entries;

            for (uint32_t i = 0; i < entry_count; ++i)
            {
                if (regions_count >= MAX_MEM_REGIONS)
                {
                    kprintf("WARNING: Reached MAX_MEM_REGIONS (%u), skipping remaining map entries.\n", MAX_MEM_REGIONS);
                    break;
                }

                uint64_t region_start = entry->addr;
                uint64_t region_end = entry->addr + entry->len;
                uint32_t region_type = entry->type;
                kprintf("  [%llx - %llx] type=%u (%llu KB)\n", region_start, region_end, region_type, entry->len / 1024);

                regions[regions_count].start = region_start;
                regions[regions_count].end = region_end;
                regions[regions_count].type = region_type;
                regions_count++;

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
                entry = (multiboot_mmap_entry *)((uint8_t *)entry + mmap_tag->entry_size);
            }
        }
        tag = (multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7));
    }

    if (!mmap_tag)
    {
        kprintf("FATAL: No memory map found from multiboot\n");
        return;
    }

    kprintf("\nTotal usable RAM: %llu MB (%llu frames)\n",
            usable_bytes / (1024 * 1024), total_frames);
    kprintf("Highest usable address: %llx (%llu MiB)\n",
            highest_usable_addr, highest_usable_addr / (1024 * 1024));

    /* Calculate bitmap size */
    uint64_t addr_space_frames = div_round_up(highest_usable_addr, PAGE_SIZE);
    uint64_t bitmap_bytes_needed = div_round_up(addr_space_frames, 8);
    bitmap_bytes_needed = align_up(bitmap_bytes_needed, PAGE_SIZE);

    const uint64_t MAX_BITMAP_SIZE = 4 * 1024 * 1024; // 4 MB - tracks up to 128GB
    if (bitmap_bytes_needed > MAX_BITMAP_SIZE)
    {
        kprintf("WARNING: Capping bitmap size from %llu KB to %llu KB\n",
                bitmap_bytes_needed / 1024, MAX_BITMAP_SIZE / 1024);
        bitmap_bytes_needed = MAX_BITMAP_SIZE;
        addr_space_frames = MAX_BITMAP_SIZE * 8;
        highest_usable_addr = addr_space_frames * PAGE_SIZE;
        kprintf("WARNING: Only tracking up to %llx (%llu MB)\n",
                highest_usable_addr, highest_usable_addr / (1024 * 1024));
    }

    kprintf("Bitmap size needed: %llu KB for %llu frames (tracking up to %llx)\n",
            bitmap_bytes_needed / 1024, addr_space_frames, highest_usable_addr);

    /* Kernel bounds */
    uint64_t kernel_phys_start = (uint64_t)&_kernel_start - KERNEL_VIRT_BASE;
    uint64_t kernel_phys_end = align_up((uint64_t)&_kernel_end - KERNEL_VIRT_BASE, PAGE_SIZE);

    kprintf("Kernel: [%llx - %llx] (%llu KB)\n",
            kernel_phys_start, kernel_phys_end,
            (kernel_phys_end - kernel_phys_start) / 1024);

    /* Reserve PT allocation area right after kernel */
    pt_reserve_start = align_up(kernel_phys_end, PAGE_SIZE);
    pt_reserve_end = pt_reserve_start + PT_RESERVE_BYTES;

    if (pt_reserve_end > EARLY_MAP_LIMIT)
    {
        kprintf("FATAL: PT reserve [%llx - %llx] exceeds MAP_LIMIT %llx\n",
                pt_reserve_start, pt_reserve_end, EARLY_MAP_LIMIT);
        return;
    }
    kprintf("PT reserve: [%llx - %llx] (%llu KB)\n",
            pt_reserve_start, pt_reserve_end, PT_RESERVE_BYTES / 1024);

    /* Build identity map up to MAP_LIMIT */
    kprintf("\nBuilding identity map up to MAP_LIMIT (%llx = %llu MB)\n",
            EARLY_MAP_LIMIT, EARLY_MAP_LIMIT / (1024 * 1024));

    uint64_t new_pml4_phys = identity_map_all(EARLY_MAP_LIMIT, pt_reserve_start, pt_reserve_end);

    uint64_t old_pml4_phys;
    __asm__ volatile("mov %%cr3, %0" : "=r"(old_pml4_phys));

    uint64_t *old_pml4 = (uint64_t *)old_pml4_phys;
    uint64_t *new_pml4 = (uint64_t *)new_pml4_phys;

    if (!new_pml4)
    {
        kprintf("FATAL: identity_map_all failed\n");
        return;
    }

    for (int i = 256; i < 512; i++)
    {
        new_pml4[i] = old_pml4[i];
    }

    kprintf("Identity map created, PML4 at %llx\n", new_pml4);
    kprintf("Switching to new page tables (CR3 = %llx)...\n", new_pml4);
    __asm__ volatile("mov %0, %%cr3" : : "r"(new_pml4));
    kprintf("CR3 switched successfully\n");

    /* Find bitmap region */
    uint64_t bitmap_phys_start = find_bitmap_region(EARLY_MAP_LIMIT, bitmap_bytes_needed, kernel_phys_start, kernel_phys_end, pt_reserve_start, pt_reserve_end, multiboot_start, multiboot_end);

    if (!bitmap_phys_start)
    {
        kprintf("FATAL: Could not find suitable bitmap region\n");
        return;
    }

    uint64_t bitmap_phys_end = bitmap_phys_start + bitmap_bytes_needed;
    kprintf("Bitmap region: [%llx - %llx] (%llu KB)\n",
            bitmap_phys_start, bitmap_phys_end, bitmap_bytes_needed / 1024);

    /* Ensure bitmap is within mapped area */
    if (bitmap_phys_end > EARLY_MAP_LIMIT)
    {
        kprintf("WARNING: Bitmap extends beyond MAP_LIMIT (%llx > %llx)\n",
                bitmap_phys_end, EARLY_MAP_LIMIT);
    }

    /* Initialize bitmap */
    bitmap_size_bytes = bitmap_bytes_needed;
    bitmap_num_frames = addr_space_frames;
    // Set bitmap pointer to Virtual Address
    bitmap = (uint8_t *)(uintptr_t)(bitmap_phys_start + KERNEL_VIRT_BASE);
    bitmap_set = 1;

    kprintf("Zeroing bitmap (%llu KB)...\n", bitmap_size_bytes / 1024);
    memset(bitmap, 0, bitmap_size_bytes);
    free_frames = bitmap_num_frames;

    kprintf("Bitmap initialized: %llu frames tracked\n", bitmap_num_frames);

    /* Reserve regions */
    kprintf("\nMarking reserved regions...\n");
    mark_region_used_internal(0, PAGE_SIZE); // frame 0
    mark_region_used_internal(kernel_phys_start, kernel_phys_end);
    mark_region_used_internal(multiboot_start, multiboot_end);
    mark_region_used_internal(pt_reserve_start, pt_reserve_end);
    mark_region_used_internal(bitmap_phys_start, bitmap_phys_end);
    mark_region_used_internal(align_down(VGA_MEMORY, PAGE_SIZE), align_up(VGA_MEMORY + (VGA_WIDTH * VGA_HEIGHT * 2), PAGE_SIZE));
    mark_region_used_internal(0x0, align_up(0x100000, PAGE_SIZE));
    /* Mark non-available regions from memory map */
    debug_kprintf("Marking non-available regions using cached map...\n");
    for (uint32_t i = 0; i < regions_count; ++i)
    {
        mem_region *region = &regions[i];

        // Check if the type is NOT available
        if (region->type != MULTIBOOT_MMAP_AVAILABLE)
        {
            // Align the region boundaries to page size
            uint64_t start = align_down(region->start, PAGE_SIZE);
            uint64_t end = align_up(region->end, PAGE_SIZE);

            // Only mark the region if it's a valid, non-zero-length block
            if (end > start)
            {
                // Mark the pages as used in the PMM bitmap
                mark_region_used_internal(start, end);

                debug_kprintf("  Used: [%llx - %llx] (Type %u)\n",
                              start, end, region->type);
            }
        }
    }

    kprintf("\n=== PMM Initialization Complete ===\n");
    pmm_print_stats();
}
