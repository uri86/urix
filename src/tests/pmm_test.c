/*
 * Licensed under MIT License - URIX project.
 * pmm_test.c - Physical Memory Manager tests.
 *
 * Responsibilities:
 *  - verify correct allocation and freeing of physical frames
 *  - ensure frame uniqueness and reuse
 *
 * Design Constraints:
 *  - Tests do NOT assume specific physical addresses.
 *  - Only ownership semantics are validated.
 */

#include <tests/__test.h>
#include <memory/physical/pmm.h>

void pmm_tests(void)
{
    TEST_BEGIN("pmm_tests");

    uint64_t a = pmm_alloc_frame();
    uint64_t b = pmm_alloc_frame();
    uint64_t c = pmm_alloc_frame();

    TEST_ASSERT_NEQ(a, 0, "alloc frame a");
    TEST_ASSERT_NEQ(b, 0, "alloc frame b");
    TEST_ASSERT_NEQ(c, 0, "alloc frame c");

    TEST_ASSERT_NEQ(a, b, "unique frames a/b");
    TEST_ASSERT_NEQ(b, c, "unique frames b/c");

    /* Free and reuse */
    pmm_free_frame(b);
    uint64_t d = pmm_alloc_frame();

    TEST_ASSERT_EQ(b, d, "reuse freed frame");

    TEST_END("pmm_tests");
}
