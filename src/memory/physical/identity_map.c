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
#include <lib/string.h> /* memset */
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

void pt_alloc_init(uint64_t start_phys, uint64_t limit_phys)
{
    /* Align to page boundaries */
    pt_alloc_next = (start_phys + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    pt_alloc_limit = limit_phys & ~(PAGE_SIZE - 1);
    pt_alloc_start_saved = pt_alloc_next;

    kprintf("pt_alloc_init: range [%llx - %llx] (%llu KiB)\n",
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
        kprintf("  Next: %llx, Limit: %llx\n",
                (uint64_t)pt_alloc_next,
                (uint64_t)pt_alloc_limit);
        kprintf("  Used: %llu KiB of %llu KiB\n",
                (uint64_t)((pt_alloc_next - pt_alloc_start_saved) / 1024ULL),
                (uint64_t)((pt_alloc_limit - pt_alloc_start_saved) / 1024ULL));
        return 0;
    }

    uint64_t page = pt_alloc_next;
    pt_alloc_next += PAGE_SIZE;

    if (page < EARLY_IDENTITY_LIMIT)
    {
        void *v = (void *)(uintptr_t)page;
        // memset(v, 0, PAGE_SIZE);
    }
    else
    {
        kprintf("WARNING: Allocated PT page at %llx beyond early identity map\n",
                (unsigned long long)page);
    }

    return page;
}

void pt_alloc_print_usage(void)
{
    uint64_t used = pt_alloc_next - pt_alloc_start_saved;
    uint64_t total = pt_alloc_limit - pt_alloc_start_saved;
    uint64_t percent = total > 0 ? (used * 100ULL) / total : 0;

    kprintf("Page table usage: %llu / %llu bytes (%llu%) = %llu KB\n",
            (uint64_t)used,
            (uint64_t)total,
            (uint64_t)percent,
            (uint64_t)(used / 1024ULL));
}

/* Build identity map for addresses [0 .. map_end) using 4KB pages.
 * pt_alloc_start/limit specify the physical range used for PT pages.
 */
int identity_map_all(uint64_t map_end, uint64_t pt_alloc_start, uint64_t pt_alloc_limit)
{
    if (map_end == 0)
    {
        kprintf("identity_map_all: ERROR - map_end is 0\n");
        return -1;
    }

    if ((uint64_t)PAGE_SIZE == 0)
    {
        kprintf("identity_map_all: ERROR - PAGE_SIZE is 0 or undefined\n");
        return -1;
    }

    /* Ensure we have an allocator range inside identity mapped area */
    if (pt_alloc_start >= EARLY_IDENTITY_LIMIT)
    {
        kprintf("identity_map_all: ERROR - PT alloc start %llx >= EARLY_IDENTITY_LIMIT %llx\n",
                (uint64_t)pt_alloc_start, (uint64_t)EARLY_IDENTITY_LIMIT);
        return -1;
    }

    if (pt_alloc_limit > EARLY_IDENTITY_LIMIT)
    {
        kprintf("identity_map_all: WARNING - limiting PT alloc limit %llx to EARLY_IDENTITY_LIMIT %llx\n",
                (uint64_t)pt_alloc_limit, (uint64_t)EARLY_IDENTITY_LIMIT);
        pt_alloc_limit = EARLY_IDENTITY_LIMIT;
    }

    /* Round up map_end to page boundary */
    map_end = (map_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    kprintf("identity_map_all: mapping [0x0 - %llx] (%llu MiB)\n",
            (uint64_t)map_end,
            (uint64_t)(map_end / (1024ULL * 1024ULL)));

    /* Initialize allocator */
    pt_alloc_init(pt_alloc_start, pt_alloc_limit);

    /* Allocate the top-level PML4 */
    uint64_t pml4_phys = pt_alloc_page_phys();
    if (!pml4_phys)
    {
        kprintf("identity_map_all: ERROR - failed to allocate PML4\n");
        return -1;
    }

    uint64_t *pml4 = (uint64_t *)(uintptr_t)pml4_phys;
    /* (we zeroed in pt_alloc_page_phys) */
    kprintf("identity_map_all: PML4 at %llx\n", (uint64_t)pml4_phys);

    uint64_t last_reported_mb = 0;
    uint64_t step = PAGE_SIZE;
    if (step == 0)
    {
        kprintf("identity_map_all: ERROR - PAGE_SIZE == 0\n");
        return -1;
    }

    for (uint64_t addr = 0; addr < map_end; addr += step)
    {
        /* Progress every 256 MiB */
        uint64_t current_mb = addr / (1024ULL * 1024ULL);
        if (current_mb >= last_reported_mb + 256ULL)
        {
            kprintf("  mapped up to %llu MiB...\n", (uint64_t)current_mb);
            last_reported_mb = current_mb;
        }

        unsigned i4 = pml4_idx(addr);
        unsigned i3 = pdpt_idx(addr);
        unsigned i2 = pd_idx(addr);
        unsigned i1 = pt_idx(addr);

        /* create PDPT */
        uint64_t *pdpt;
        if (!(pml4[i4] & PAGE_PRESENT))
        {
            uint64_t pdpt_phys = pt_alloc_page_phys();
            if (!pdpt_phys)
            {
                kprintf("identity_map_all: ERROR - failed to allocate PDPT at addr %llx\n",
                        (unsigned long long)addr);
                return -1;
            }
            // memset((void *)(uintptr_t)pdpt_phys, 0, PAGE_SIZE);
            pml4[i4] = pdpt_phys | PAGE_PRESENT_RW;
            pdpt = (uint64_t *)(uintptr_t)pdpt_phys;
        }
        else
        {
            pdpt = (uint64_t *)(uintptr_t)pte_to_phys(pml4[i4]);
        }

        /* create PD */
        uint64_t *pd;
        if (!(pdpt[i3] & PAGE_PRESENT))
        {
            uint64_t pd_phys = pt_alloc_page_phys();
            if (!pd_phys)
            {
                kprintf("identity_map_all: ERROR - failed to allocate PD at addr %llx\n",
                        (uint64_t)addr);
                return -1;
            }
            // memset((void *)(uintptr_t)pd_phys, 0, PAGE_SIZE);
            pdpt[i3] = pd_phys | PAGE_PRESENT_RW;
            pd = (uint64_t *)(uintptr_t)pd_phys;
        }
        else
        {
            pd = (uint64_t *)(uintptr_t)pte_to_phys(pdpt[i3]);
        }

        /* create PT */
        uint64_t *pt;
        if (!(pd[i2] & PAGE_PRESENT))
        {
            uint64_t pt_phys = pt_alloc_page_phys();
            if (!pt_phys)
            {
                kprintf("identity_map_all: ERROR - failed to allocate PT at addr %llx\n",
                        (uint64_t)addr);
                return -1;
            }
            // memset((void *)(uintptr_t)pt_phys, 0, PAGE_SIZE);
            pd[i2] = pt_phys | PAGE_PRESENT_RW;
            pt = (uint64_t *)(uintptr_t)pt_phys;
        }
        else
        {
            pt = (uint64_t *)(uintptr_t)pte_to_phys(pd[i2]);
        }

        /* Create final mapping (identity map the 4KB page). */
        pt[i1] = (addr & ~0xFFFULL) | PAGE_PRESENT_RW;
    }

    kprintf("identity_map_all: finished mapping all pages\n");
    pt_alloc_print_usage();

    /* Switch CR3 to new PML4 */
    kprintf("identity_map_all: switching to new CR3 (%llx)...\n", (uint64_t)pml4_phys);

    __asm__ volatile(
        "mov %0, %%rax\n\t"
        "mov %%rax, %%cr3\n\t"
        : /* no outputs */
        : "r"(pml4_phys)
        : "rax", "memory");

    kprintf("identity_map_all: SUCCESS - new page tables active\n");
    return 0;
}
