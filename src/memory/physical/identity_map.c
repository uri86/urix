/*
 * Licensed under MIT License - URIX project.
 * identity_map.c - Build 4-level page tables for identity mapping (4KB pages).
 * Responsibilities:
 *  - allocate pages for page tables within a given physical range
 *  - build PML4/PDPT/PD/PT hierarchy for identity mapping
 *  - map virtual addresses to the same physical addresses for low memory
 *  - provide diagnostic printing of allocator and mapping progress
 * Notes:
 *  - relies on early identity mapping for addresses below EARLY_IDENTITY_LIMIT
 *  - zeroing of allocated pages is skipped if beyond early identity map
 *  - includes helper functions to extract indices and physical addresses from PTEs
 *  - switches CR3 to new PML4 after mapping completion
 */

#include <stdint.h>
#include <stddef.h>
#include <lib/print.h>
#include <string.h>
#include <memory/physical/pmm.h>
#include <memory/physical/identity_map.h>

/* Page table allocator state */
static uint64_t pt_alloc_next = 0;
static uint64_t pt_alloc_limit = 0;
static uint64_t pt_alloc_start_saved = 0;

static inline unsigned pml4_idx(uint64_t addr) { return (addr >> 39) & 0x1FF; }
static inline unsigned pdpt_idx(uint64_t addr) { return (addr >> 30) & 0x1FF; }
static inline unsigned pd_idx(uint64_t addr) { return (addr >> 21) & 0x1FF; }
static inline unsigned pt_idx(uint64_t addr) { return (addr >> 12) & 0x1FF; }

/* Extract physical address from PTE (clear flags) */
static inline uint64_t pte_to_phys(uint64_t entry) { return entry & 0x000FFFFFFFFFF000ULL; }

/* Helper to get or allocate a generic mapped page table level */
static inline uint64_t *get_or_alloc_table(uint64_t *parent_table, unsigned index, uint64_t addr, const char *level_name)
{
    if (!(parent_table[index] & PAGE_PRESENT))
    {
        uint64_t phys = pt_alloc_page_phys();
        if (!phys)
        {
            kprintf("identity_map_all: ERROR - failed to allocate %s at addr 0x%llx\n", level_name, (uint64_t)addr);
            return NULL;
        }
        parent_table[index] = phys | PAGE_PRESENT_RW;
        return (uint64_t *)(uintptr_t)phys;
    }
    return (uint64_t *)(uintptr_t)pte_to_phys(parent_table[index]);
}

void pt_alloc_init(uint64_t start_phys, uint64_t limit_phys)
{
    /* Align to page boundaries */
    pt_alloc_next = (start_phys + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    pt_alloc_limit = limit_phys & ~(PAGE_SIZE - 1);
    pt_alloc_start_saved = pt_alloc_next;

    kprintf("pt_alloc_init: range [0x%llx - 0x%llx] (%llu KiB)\n",
            (uint64_t)pt_alloc_next,
            (uint64_t)pt_alloc_limit,
            (uint64_t)((pt_alloc_limit - pt_alloc_next) / 1024ULL));
}

uint64_t pt_alloc_page_phys(void)
{
    /* Check available space */
    if (pt_alloc_next + PAGE_SIZE > pt_alloc_limit)
    {
        kprintf("CRITICAL: Page table allocator exhausted\n");
        kprintf("  Next: 0x%llx, Limit: 0x%llx\n",
                (uint64_t)pt_alloc_next,
                (uint64_t)pt_alloc_limit);
        kprintf("  Used: %llu KiB of %llu KiB\n",
                (uint64_t)((pt_alloc_next - pt_alloc_start_saved) / 1024ULL),
                (uint64_t)((pt_alloc_limit - pt_alloc_start_saved) / 1024ULL));
        return 0;
    }

    uint64_t page = pt_alloc_next;
    pt_alloc_next += PAGE_SIZE;

    /* Only zero if we can safely access it (within early identity map) */
    if (page < EARLY_IDENTITY_LIMIT)
    {
        void *v = (void *)(uintptr_t)page;
        memset(v, 0, PAGE_SIZE);
    }
    else
    {
        /* This should never happen if PT reserve is within EARLY_IDENTITY_LIMIT */
        kprintf("CRITICAL ERROR: PT page at 0x%llx is beyond EARLY_IDENTITY_LIMIT 0x%llx\n",
                (unsigned long long)page, (unsigned long long)EARLY_IDENTITY_LIMIT);
        return 0;
    }

    return page;
}

void pt_alloc_print_usage(void)
{
    uint64_t used = pt_alloc_next - pt_alloc_start_saved;
    uint64_t total = pt_alloc_limit - pt_alloc_start_saved;
    uint64_t percent = total > 0 ? (used * 100ULL) / total : 0;

    kprintf("Page table usage: %llu / %llu bytes (%llu %) = %llu KB\n",
            (uint64_t)used,
            (uint64_t)total,
            (uint64_t)percent,
            (uint64_t)(used / 1024ULL));
}

/* Build identity map for addresses [0 .. map_end) using 4KB pages.
 * pt_alloc_start/limit specify the physical range used for PT pages.
 * Returns the physical address of the new PML4, or 0 on failure.
 * NOTE: Does NOT switch CR3 - caller must do that when ready.
 */
uint64_t identity_map_all(uint64_t map_end, uint64_t pt_alloc_start, uint64_t pt_alloc_limit)
{
    if (map_end == 0)
    {
        kprintf("identity_map_all: ERROR - map_end is 0\n");
        return 0;
    }

    if (PAGE_SIZE == 0)
    {
        kprintf("identity_map_all: ERROR - PAGE_SIZE is 0 or undefined\n");
        return 0;
    }

    /* Ensure allocator range is within identity-mapped area */
    if (pt_alloc_start >= EARLY_IDENTITY_LIMIT)
    {
        kprintf("identity_map_all: ERROR - PT alloc start 0x%llx >= EARLY_IDENTITY_LIMIT 0x%llx\n",
                (uint64_t)pt_alloc_start, (uint64_t)EARLY_IDENTITY_LIMIT);
        return 0;
    }

    if (pt_alloc_limit > EARLY_IDENTITY_LIMIT)
    {
        kprintf("identity_map_all: WARNING - PT alloc limit 0x%llx exceeds EARLY_IDENTITY_LIMIT 0x%llx, clamping\n",
                (uint64_t)pt_alloc_limit, (uint64_t)EARLY_IDENTITY_LIMIT);
        pt_alloc_limit = EARLY_IDENTITY_LIMIT;
    }

    /* Round up map_end to page boundary */
    map_end = (map_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    kprintf("identity_map_all: mapping [0x0 - 0x%llx] (%llu MiB)\n",
            (uint64_t)map_end,
            (uint64_t)(map_end / (1024ULL * 1024ULL)));

    /* Initialize allocator */
    pt_alloc_init(pt_alloc_start, pt_alloc_limit);

    /* Allocate the top-level PML4 */
    uint64_t pml4_phys = pt_alloc_page_phys();
    if (!pml4_phys)
    {
        kprintf("identity_map_all: ERROR - failed to allocate PML4\n");
        return 0;
    }

    uint64_t *pml4 = (uint64_t *)(uintptr_t)pml4_phys;
    kprintf("identity_map_all: PML4 allocated at phys 0x%llx\n", (uint64_t)pml4_phys);

    uint64_t last_reported_mb = 0;
    uint64_t pages_mapped = 0;

    for (uint64_t addr = 0; addr < map_end; addr += PAGE_SIZE)
    {
        /* Progress reporting every 256 MiB */
        uint64_t current_mb = addr / (1024ULL * 1024ULL);
        if (current_mb >= last_reported_mb + 256ULL)
        {
            kprintf("  mapped up to 0x%llx MiB (0x%llx pages)...\n",
                    (uint64_t)current_mb, (uint64_t)pages_mapped);
            last_reported_mb = current_mb;
        }

        unsigned i4 = pml4_idx(addr);
        unsigned i3 = pdpt_idx(addr);
        unsigned i2 = pd_idx(addr);
        unsigned i1 = pt_idx(addr);

        /* Get or create PDPT */
        uint64_t *pdpt = get_or_alloc_table(pml4, i4, addr, "PDPT");
        if (!pdpt) return 0;

        /* Get or create PD */
        uint64_t *pd = get_or_alloc_table(pdpt, i3, addr, "PD");
        if (!pd) return 0;

        /* Get or create PT */
        uint64_t *pt = get_or_alloc_table(pd, i2, addr, "PT");
        if (!pt) return 0;

        /* Create final 4KB page mapping (identity: VA = PA) */
        if (!(pt[i1] & PAGE_PRESENT))
        {
            pt[i1] = (addr & ~0xFFFULL) | PAGE_PRESENT_RW;
            pages_mapped++;
        }
    }

    kprintf("identity_map_all: finished mapping %llu pages\n", (uint64_t)pages_mapped);
    pt_alloc_print_usage();

    /* DO NOT switch CR3 here - let caller do it when ready */
    kprintf("identity_map_all: SUCCESS - returning PML4 at phys 0x%llx\n", (uint64_t)pml4_phys);
    kprintf("identity_map_all: Caller must switch CR3 when ready\n");

    return pml4_phys;
}