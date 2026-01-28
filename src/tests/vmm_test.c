/*
 * Licensed under MIT License - URIX project.
 * vmm_test.c - Virtual Memory Manager tests.
 *
 * Responsibilities:
 *  - verify virtual-to-physical mappings
 *  - validate page table walking and unmapping
 */

#include <tests/__test.h>
#include <memory/vmm.h>
#include <memory/physical/pmm.h>

void vmm_tests(void)
{
    TEST_BEGIN("vmm_tests");

    uint64_t phys = pmm_alloc_frame();
    TEST_ASSERT_NEQ(phys, 0, "alloc phys frame");

    uint64_t virt = 0xFFFFFFFF90000000ULL;

    TEST_ASSERT_EQ(
        vmm_map_page(NULL, virt, phys, VMM_KERNEL_FLAGS),
        0,
        "map page");

    TEST_ASSERT_EQ(
        vmm_get_physical(NULL, virt),
        phys,
        "resolve mapping");

    TEST_ASSERT_EQ(
        vmm_unmap_page(NULL, virt),
        phys,
        "unmap page");

    TEST_ASSERT_EQ(
        vmm_get_physical(NULL, virt),
        0,
        "verify unmapped");

    pmm_free_frame(phys);

    TEST_END("vmm_tests");
}
