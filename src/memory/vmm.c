/*
 * Licensed under MIT License - URIX project.
 * vmm.c - Virtual Memory Manager implementation.
 */

#include <memory/vmm.h>
#include <memory/physical/pmm.h>
#include <lib/print.h>
#include <string.h>
#include <lib/panic.h>

/*
 * Global kernel address space
 * All processes copy kernel mappings from this space
 */
static address_space_t kernel_space;

/*
 * Current active address space (optimization)
 * Tracks which address space is currently loaded in CR3
 */
static address_space_t *current_space = NULL;

/*
 * Flag: are we in higher-half mode?
 * Before transition: kernel at 0x100000 (identity)
 * After transition: kernel at 0xFFFF800000100000 (higher-half)
 */
static int in_higher_half = 0;

/*
 * Extract page table indices from virtual address
 *
 * x86-64 virtual address (48-bit):
 *   Bits 39-47: PML4 index (9 bits) → 512 entries
 *   Bits 30-38: PDPT index (9 bits) → 512 entries
 *   Bits 21-29: PD index   (9 bits) → 512 entries
 *   Bits 12-20: PT index   (9 bits) → 512 entries
 *   Bits 0-11:  Page offset (12 bits) → 4KB page
 */
static inline uint64_t pml4_index(uint64_t vaddr) { return (vaddr >> 39) & 0x1FF; }
static inline uint64_t pdpt_index(uint64_t vaddr) { return (vaddr >> 30) & 0x1FF; }
static inline uint64_t pd_index(uint64_t vaddr) { return (vaddr >> 21) & 0x1FF; }
static inline uint64_t pt_index(uint64_t vaddr) { return (vaddr >> 12) & 0x1FF; }

/*
 * Extract physical address from page table entry
 * Clears all flag bits (lower 12 bits) to get just the address
 */
static inline uint64_t pte_to_phys(uint64_t pte)
{
    return pte & 0x000FFFFFFFFFF000ULL;
}

/*
 * Convert physical to virtual
 *
 * Before higher-half: returns identity-mapped address
 * After higher-half: returns KERNEL_VIRT_BASE + phys
 */
static inline void *phys_to_virt_internal(uint64_t phys)
{
    if (in_higher_half)
    {
        return (void *)(phys + KERNEL_VIRT_BASE);
    }
    else
    {
        return (void *)phys; /* Identity mapping */
    }
}

/*
 * Convert virtual to physical
 *
 * Before higher-half: assumes identity mapping
 * After higher-half: subtracts KERNEL_VIRT_BASE
 */
static inline uint64_t virt_to_phys_internal(void *virt)
{
    uint64_t addr = (uint64_t)virt;
    if (in_higher_half)
    {
        return addr - KERNEL_VIRT_BASE;
    }
    else
    {
        return addr; /* Identity mapping */
    }
}

void vmm_init(void)
{
    kprintf("\n=== Initializing VMM ===\n");

    /*
     * Read current CR3
     * This is the identity-mapped PML4 created by PMM during boot
     */
    uint64_t current_pml4_phys;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_pml4_phys));
    kprintf("Current PML4 (identity): %llx\n", current_pml4_phys);

    /*
     * Allocate new PML4 for kernel space
     * This will have both identity mapping AND higher-half mapping
     */
    kernel_space.pml4_phys = pmm_alloc_frame();
    if (!kernel_space.pml4_phys)
    {
        kprintf("FATAL: Failed to allocate kernel PML4\n");
        PANIC("Failed to allocate kernel PML4 - Out of Memory");
    }

    /* Access PML4 through identity mapping (still in low addresses) */
    kernel_space.pml4_virt = (uint64_t *)kernel_space.pml4_phys;
    memset(kernel_space.pml4_virt, 0, PAGE_SIZE);

    kprintf("Kernel PML4 allocated at: %llx\n", kernel_space.pml4_phys);

    /*
     * Copy existing identity mapping
     * This ensures kernel code continues to work after switching CR3
     */
    uint64_t *old_pml4 = (uint64_t *)current_pml4_phys;
    for (int i = 0; i < 256; i++)
    { /* Lower half only */
        kernel_space.pml4_virt[i] = old_pml4[i];
    }
    kprintf("Copied identity mapping to new PML4\n");

    /*
     * Create higher-half mapping
     * Map first 2GB of physical memory to KERNEL_VIRT_BASE
     * This allows us to access all kernel memory at high addresses
     */
    uint64_t map_size = 2ULL * 1024 * 1024 * 1024; /* 2GB */
    kprintf("Creating higher-half mapping (2GB at %llx)...\n", KERNEL_VIRT_BASE);

    uint64_t pages_mapped = 0;
    for (uint64_t phys = 0; phys < map_size; phys += PAGE_SIZE)
    {
        uint64_t virt = KERNEL_VIRT_BASE + phys;

        if (vmm_map_page(&kernel_space, virt, phys, VMM_KERNEL_FLAGS) != 0)
        {
            kprintf("FATAL: Failed to map page at virt=%llx phys=%llx\n", virt, phys);
            PANIC("Failed to map higher-half kernel page");
        }

        pages_mapped++;

        /* Progress indicator every 64MB */
        if (pages_mapped % (64 * 1024 * 1024 / PAGE_SIZE) == 0)
        {
            kprintf("  Mapped %llu MB...\n", (phys / (1024 * 1024)));
        }
    }

    kprintf("Higher-half mapping complete (%llu pages)\n", pages_mapped);

    /*
     * Switch to new page tables
     * After this, we have BOTH identity and higher-half mappings active
     */
    kprintf("Switching to new page tables (CR3 = %llx)...\n", kernel_space.pml4_phys);
    __asm__ volatile("mov %0, %%cr3" : : "r"(kernel_space.pml4_phys) : "memory");
    current_space = &kernel_space;

    kprintf("Page tables switched successfully\n");
    kprintf("Identity mapping: 0x0 - %llx\n", map_size);
    kprintf("Higher-half mapping: %llx - %llx\n",
            KERNEL_VIRT_BASE, KERNEL_VIRT_BASE + map_size);

    kprintf("VMM initialized (dual mapping active)\n");
    kprintf("========================\n\n");
}

void vmm_finish_init(void)
{
    kprintf("\n=== Completing VMM Setup ===\n");

    if (in_higher_half)
    {
        kprintf("Already in higher-half mode\n");
        return;
    }

    /*
     * Remove identity mapping (lower half)
     * After this, only higher-half addresses work
     * User space (lower half) becomes available for processes
     */
    kernel_space.pml4_virt = (uint64_t *)phys_to_virt(kernel_space.pml4_phys);

    // Now clear the low mappings
    for (int i = 0; i < 256; i++)
    {
        kernel_space.pml4_virt[i] = 0;
    }

    // Flush TLB (reload CR3) - use the PHYSICAL address for CR3
    __asm__ volatile("mov %0, %%cr3" : : "r"(kernel_space.pml4_phys) : "memory");

    in_higher_half = 1;

    kprintf("Switched to higher-half mode\n");
    kprintf("Lower half (0x0 - 0x7FFF...) now available for user processes\n");
    kprintf("============================\n\n");
}

address_space_t *vmm_create_address_space(void)
{
    /* Allocate address_space_t structure */
    address_space_t *as = (address_space_t *)phys_to_virt_internal(pmm_alloc_frame());
    if (!as)
    {
        kprintf("vmm_create_address_space: Failed to allocate structure\n");
        return NULL;
    }

    /* Allocate PML4 */
    as->pml4_phys = pmm_alloc_frame();
    if (!as->pml4_phys)
    {
        pmm_free_frame(virt_to_phys_internal(as));
        kprintf("vmm_create_address_space: Failed to allocate PML4\n");
        return NULL;
    }

    as->pml4_virt = (uint64_t *)phys_to_virt_internal(as->pml4_phys);
    memset(as->pml4_virt, 0, PAGE_SIZE);

    /*
     * Copy kernel mappings (upper half) from kernel space
     * This ensures all processes can access kernel code/data
     */
    uint64_t *kernel_pml4 = kernel_space.pml4_virt;
    for (int i = 256; i < 512; i++)
    { /* Upper half only */
        as->pml4_virt[i] = kernel_pml4[i];
    }

    return as;
}

int vmm_clone_user_space(address_space_t *src, address_space_t *dst)
{
    if (!src || !dst)
        return -1;

    __asm__ volatile("cli");

    uint64_t *src_pml4 = src->pml4_virt;

    for (int i4 = 0; i4 < 256; i4++)
    {
        if (!(src_pml4[i4] & VMM_PRESENT))
            continue;

        uint64_t *src_pdpt = (uint64_t *)phys_to_virt(pte_to_phys(src_pml4[i4]));

        for (int i3 = 0; i3 < 512; i3++)
        {
            if (!(src_pdpt[i3] & VMM_PRESENT))
                continue;

            uint64_t *src_pd = (uint64_t *)phys_to_virt(pte_to_phys(src_pdpt[i3]));

            for (int i2 = 0; i2 < 512; i2++)
            {
                if (!(src_pd[i2] & VMM_PRESENT))
                    continue;

                uint64_t *src_pt = (uint64_t *)phys_to_virt(pte_to_phys(src_pd[i2]));

                for (int i1 = 0; i1 < 512; i1++)
                {
                    if (!(src_pt[i1] & VMM_PRESENT))
                        continue;

                    uint64_t src_phys = pte_to_phys(src_pt[i1]);
                    uint64_t flags = src_pt[i1] & 0xFFF;

                    uint64_t virt = ((uint64_t)i4 << 39) |
                                    ((uint64_t)i3 << 30) |
                                    ((uint64_t)i2 << 21) |
                                    ((uint64_t)i1 << 12);

                    uint64_t dst_phys = pmm_alloc_frame();
                    if (!dst_phys)
                    {
                        debug_kprintf("vmm_clone_user_space: OOM at virt %llx\n", virt);
                        __asm__ volatile("sti");
                        return -1;
                    }

                    uint64_t dst_virt = (uint64_t)phys_to_virt(dst_phys);
                    vmm_map_kernel_page(dst_virt, dst_phys);
                    memcpy((void *)dst_virt, phys_to_virt(src_phys), PAGE_SIZE);

                    if (vmm_map_page(dst, virt, dst_phys, flags) != 0)
                    {
                        debug_kprintf("vmm_clone_user_space: map failed at virt %llx\n", virt);
                        pmm_free_frame(dst_phys);
                        __asm__ volatile("sti");
                        return -1;
                    }
                }
            }
        }
    }

    __asm__ volatile("sti");
    return 0;
}

void vmm_destroy_address_space(address_space_t *as)
{
    if (!as || as == &kernel_space)
    {
        kprintf("vmm_destroy_address_space: Invalid address space\n");
        return;
    }

    uint64_t *pml4 = as->pml4_virt;

    for (int i4 = 0; i4 < 256; i4++)
    {
        if (!(pml4[i4] & VMM_PRESENT))
            continue;

        uint64_t *pdpt = (uint64_t *)phys_to_virt_internal(pte_to_phys(pml4[i4]));

        for (int i3 = 0; i3 < 512; i3++)
        {
            if (!(pdpt[i3] & VMM_PRESENT))
                continue;

            uint64_t *pd = (uint64_t *)phys_to_virt_internal(pte_to_phys(pdpt[i3]));

            for (int i2 = 0; i2 < 512; i2++)
            {
                if (!(pd[i2] & VMM_PRESENT))
                    continue;

                uint64_t *pt = (uint64_t *)phys_to_virt_internal(pte_to_phys(pd[i2]));

                /* Free every data page the PT points to */
                for (int i1 = 0; i1 < 512; i1++)
                {
                    if (!(pt[i1] & VMM_PRESENT))
                        continue;
                    pmm_free_frame(pte_to_phys(pt[i1]));
                }

                /* Free PT */
                pmm_free_frame(pte_to_phys(pd[i2]));
            }

            /* FreePD */
            pmm_free_frame(pte_to_phys(pdpt[i3]));
        }

        /* Free PDPT */
        pmm_free_frame(pte_to_phys(pml4[i4]));
    }

    /* Free PML4 frame and the address_space_t struct frame */
    pmm_free_frame(as->pml4_phys);
    pmm_free_frame(virt_to_phys_internal(as));
}
void vmm_switch_address_space(address_space_t *as)
{
    if (!as)
        as = &kernel_space;

    /* Load PML4 into CR3 */
    __asm__ volatile("mov %0, %%cr3" : : "r"(as->pml4_phys) : "memory");
    current_space = as;
}

int vmm_map_page(address_space_t *as, uint64_t virt_addr,
                 uint64_t phys_addr, uint64_t flags)
{
    if (!as)
        as = current_space ? current_space : &kernel_space;

    /* Ensure addresses are page-aligned */
    if ((virt_addr & 0xFFF) || (phys_addr & 0xFFF))
    {
        kprintf("vmm_map_page: Addresses must be page-aligned\n");
        return -1;
    }

    uint64_t table_flags = VMM_PRESENT | VMM_WRITE;
    if (flags & VMM_USER)
        table_flags |= VMM_USER;

    uint64_t *pml4 = as->pml4_virt;

    /* Get/create PDPT */
    uint64_t i4 = pml4_index(virt_addr);
    uint64_t *pdpt;
    if (!(pml4[i4] & VMM_PRESENT))
    {
        uint64_t pdpt_phys = pmm_alloc_frame();
        if (!pdpt_phys)
            return -1;
        pml4[i4] = pdpt_phys | table_flags;
        pdpt = (uint64_t *)phys_to_virt_internal(pdpt_phys);
        memset(pdpt, 0, PAGE_SIZE);
    }
    else
    {
        if ((flags & VMM_USER) && !(pml4[i4] & VMM_USER))
            pml4[i4] |= VMM_USER;
        pdpt = (uint64_t *)phys_to_virt_internal(pte_to_phys(pml4[i4]));
    }

    /* Get/create PD */
    uint64_t i3 = pdpt_index(virt_addr);
    uint64_t *pd;
    if (!(pdpt[i3] & VMM_PRESENT))
    {
        uint64_t pd_phys = pmm_alloc_frame();
        if (!pd_phys)
            return -1;
        pdpt[i3] = pd_phys | table_flags;
        pd = (uint64_t *)phys_to_virt_internal(pd_phys);
        memset(pd, 0, PAGE_SIZE);
    }
    else
    {
        if ((flags & VMM_USER) && !(pdpt[i3] & VMM_USER))
            pdpt[i3] |= VMM_USER;
        pd = (uint64_t *)phys_to_virt_internal(pte_to_phys(pdpt[i3]));
    }

    /* Get/create PT */
    uint64_t i2 = pd_index(virt_addr);
    uint64_t *pt;
    if (!(pd[i2] & VMM_PRESENT))
    {
        uint64_t pt_phys = pmm_alloc_frame();
        if (!pt_phys)
            return -1;
        pd[i2] = pt_phys | table_flags;
        pt = (uint64_t *)phys_to_virt_internal(pt_phys);
        memset(pt, 0, PAGE_SIZE);
    }
    else
    {
        if ((flags & VMM_USER) && !(pd[i2] & VMM_USER))
            pd[i2] |= VMM_USER;
        pt = (uint64_t *)phys_to_virt_internal(pte_to_phys(pd[i2]));
    }

    /* Set PTE */
    uint64_t i1 = pt_index(virt_addr);
    pt[i1] = (phys_addr & ~0xFFFULL) | flags;

    return 0;
}

uint64_t vmm_unmap_page(address_space_t *as, uint64_t virt_addr)
{
    if (!as)
        as = current_space ? current_space : &kernel_space;

    uint64_t *pml4 = as->pml4_virt;

    /* Walk page tables */
    uint64_t i4 = pml4_index(virt_addr);
    if (!(pml4[i4] & VMM_PRESENT))
        return 0;

    uint64_t *pdpt = (uint64_t *)phys_to_virt_internal(pte_to_phys(pml4[i4]));
    uint64_t i3 = pdpt_index(virt_addr);
    if (!(pdpt[i3] & VMM_PRESENT))
        return 0;

    uint64_t *pd = (uint64_t *)phys_to_virt_internal(pte_to_phys(pdpt[i3]));
    uint64_t i2 = pd_index(virt_addr);
    if (!(pd[i2] & VMM_PRESENT))
        return 0;

    uint64_t *pt = (uint64_t *)phys_to_virt_internal(pte_to_phys(pd[i2]));
    uint64_t i1 = pt_index(virt_addr);

    /* Get physical address and clear entry */
    uint64_t phys = pte_to_phys(pt[i1]);
    pt[i1] = 0;

    /* Invalidate TLB for this address */
    __asm__ volatile("invlpg (%0)" : : "r"(virt_addr) : "memory");

    return phys;
}

void vmm_map_kernel_page(uint64_t virt, uint64_t phys)
{
    address_space_t *kernel_as = vmm_get_kernel_space();

    vmm_map_page(kernel_as, virt, phys, VMM_KERNEL_FLAGS);

    uint64_t current_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));

    if (current_cr3 != kernel_as->pml4_phys)
        vmm_map_page(NULL, virt, phys, VMM_KERNEL_FLAGS);
}

void vmm_unmap_kernel_page(uint64_t virt)
{
    address_space_t *kernel_as = vmm_get_kernel_space();

    uint64_t phys = vmm_unmap_page(kernel_as, virt);

    uint64_t current_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));

    if (current_cr3 != kernel_as->pml4_phys)
        vmm_unmap_page(NULL, virt);

    if (phys)
        pmm_free_frame(phys);
}

uint64_t vmm_get_physical(address_space_t *as, uint64_t virt_addr)
{
    if (!as)
        as = current_space ? current_space : &kernel_space;

    uint64_t *pml4 = as->pml4_virt;

    /* Walk page tables */
    uint64_t i4 = pml4_index(virt_addr);
    if (!(pml4[i4] & VMM_PRESENT))
        return 0;

    uint64_t *pdpt = (uint64_t *)phys_to_virt_internal(pte_to_phys(pml4[i4]));
    uint64_t i3 = pdpt_index(virt_addr);
    if (!(pdpt[i3] & VMM_PRESENT))
        return 0;

    uint64_t *pd = (uint64_t *)phys_to_virt_internal(pte_to_phys(pdpt[i3]));
    uint64_t i2 = pd_index(virt_addr);
    if (!(pd[i2] & VMM_PRESENT))
        return 0;

    uint64_t *pt = (uint64_t *)phys_to_virt_internal(pte_to_phys(pd[i2]));
    uint64_t i1 = pt_index(virt_addr);

    /* Return physical address with page offset */
    return pte_to_phys(pt[i1]) | (virt_addr & 0xFFF);
}

address_space_t *vmm_get_kernel_space(void)
{
    return &kernel_space;
}