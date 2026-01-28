/*
 * Licensed under MIT License - URIX project.
 * vmm.h - Virtual Memory Manager interface.
 * Responsibilities:
 *  - manage per-process virtual address spaces
 *  - provide page table manipulation functions
 *  - handle virtual-to-physical mappings
 *  - support kernel and user address spaces
 *  - transition kernel to higher-half mapping
 * Notes:
 *  - uses 4-level paging (PML4/PDPT/PD/PT)
 *  - each process has its own PML4
 *  - identity mapping kept during transition for safety
 */

#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>

#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000ULL /* Start of higher-half */
#define USER_VIRT_BASE 0x0000000000400000ULL   /* 4MB - standard ELF load address */
#define USER_STACK_TOP 0x0000800000000000ULL
#define KERNEL_STACK_SIZE (16384ULL)

/* Page table entry flags */
#define VMM_PRESENT 0x001ULL
#define VMM_WRITE 0x002ULL
#define VMM_USER 0x004ULL
#define VMM_WRITETHROUGH 0x008ULL
#define VMM_CACHE_DISABLE 0x010ULL
#define VMM_ACCESSED 0x020ULL
#define VMM_DIRTY 0x040ULL
#define VMM_HUGE 0x080ULL
#define VMM_GLOBAL 0x100ULL
#define VMM_NX 0x8000000000000000ULL

/* Common flag combinations */
#define VMM_KERNEL_FLAGS (VMM_PRESENT | VMM_WRITE)
#define VMM_USER_FLAGS (VMM_PRESENT | VMM_WRITE | VMM_USER)

/*
 * Address Space Structure
 *
 * Represents a complete virtual address space.
 */
typedef struct address_space
{
    uint64_t pml4_phys;  /* Physical address of PML4 */
    uint64_t *pml4_virt; /* Virtual address of PML4 */
} address_space_t;

/**
 * vmm_init - Initialize the Virtual Memory Manager
 *
 * This function:
 *  1. Creates kernel address space with higher-half mapping
 *  2. Maps all physical memory to KERNEL_VIRT_BASE
 *  3. Keeps identity mapping during transition for safety
 */
void vmm_init(void);

/**
 * vmm_finish_init - Complete VMM initialization and switch to higher-half
 *
 * This function:
 *  1. Removes identity mapping
 *  2. Updates all kernel pointers to use higher-half addresses
 *  3. Flushes TLB
 */
void vmm_finish_init(void);

/**
 * vmm_create_address_space - Create a new address space for a process
 *
 * Creates a new PML4 and copies kernel mappings into it.
 * User space is initially empty.
 *
 * Returns pointer to address_space_t or NULL on failure.
 */
address_space_t *vmm_create_address_space(void);

/**
 * vmm_destroy_address_space - Destroy an address space
 *
 * Frees all page tables in user space.
 * Does NOT touch kernel mappings (shared across all processes).
 *
 * as: address space to destroy (must not be currently active)
 */
void vmm_destroy_address_space(address_space_t *as);

/**
 * vmm_switch_address_space - Switch to a different address space
 *
 * Loads the PML4 into CR3, changing the active page tables.
 * Kernel mappings remain the same.
 * User mappings change.
 *
 * as: address space to switch to (NULL = kernel space only)
 */
void vmm_switch_address_space(address_space_t *as);

/**
 * vmm_map_page - Map a virtual page to a physical frame
 *
 * Creates page table entries to map a virtual address to physical address.
 * Allocates intermediate page tables (PDPT, PD, PT) as needed.
 *
 * as: address space (NULL = current/kernel)
 * virt_addr: virtual address to map (must be page-aligned)
 * phys_addr: physical address to map to (must be page-aligned)
 * flags: page flags
 *
 * Returns 0 on success, -1 on failure.
 */
int vmm_map_page(address_space_t *as, uint64_t virt_addr,
                 uint64_t phys_addr, uint64_t flags);

/**
 * vmm_unmap_page - Unmap a virtual page
 *
 * Removes the page table entry for a virtual address.
 * Does NOT free the physical frame (caller's responsibility).
 * Invalidates TLB for this address.
 *
 * as: address space (NULL = current/kernel)
 * virt_addr: virtual address to unmap
 *
 * Returns the physical address that was mapped, or 0 if not mapped.
 */
uint64_t vmm_unmap_page(address_space_t *as, uint64_t virt_addr);

/**
 * vmm_get_physical - Get physical address for a virtual address
 *
 * Walks page tables to find the physical address mapped to a virtual address.
 *
 * as: address space (NULL = current/kernel)
 * virt_addr: virtual address to look up
 *
 * Returns physical address or 0 if not mapped.
 */
uint64_t vmm_get_physical(address_space_t *as, uint64_t virt_addr);

/**
 * vmm_get_kernel_space - Get the kernel address space
 *
 * Returns pointer to the global kernel address_space_t.
 */
address_space_t *vmm_get_kernel_space(void);

/**
 * phys_to_virt - Convert physical address to virtual (kernel direct map)
 *
 * Uses the direct mapping at KERNEL_VIRT_BASE.
 * Only works for addresses within mapped physical memory.
 *
 * Example: phys_to_virt(0x100000) → 0xFFFFFFFF80100000
 */
static inline void *phys_to_virt(uint64_t phys)
{
    return (void *)(phys + KERNEL_VIRT_BASE);
}

/**
 * virt_to_phys - Convert virtual address to physical (kernel direct map)
 *
 * Only works for kernel direct-mapped addresses (KERNEL_VIRT_BASE+).
 * Does NOT work for arbitrary virtual addresses.
 *
 * Example: virt_to_phys(0xFFFFFFFF80100000) → 0x100000
 */
static inline uint64_t virt_to_phys(void *virt)
{
    return (uint64_t)virt - KERNEL_VIRT_BASE;
}

/**
 * vmm_is_kernel_address - Check if address is in kernel space
 *
 * Returns 1 if address is in upper half (kernel space).
 * Returns 0 if address is in lower half (user space).
 */
static inline int vmm_is_kernel_address(uint64_t virt)
{
    return virt >= KERNEL_VIRT_BASE;
}

#endif /* VMM_H */