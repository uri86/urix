/*
 * Licensed under MIT License - URIX project.
 * __test.h - Internal kernel test framework helpers.
 *
 * Responsibilities:
 *  - provide assertion macros for kernel tests
 *  - provide standardized PASS/FAIL reporting
 *  - enforce fail-fast behavior on test failure
 *
 * Failure Model:
 *  - Any failed assertion prints diagnostic information
 *  - The kernel halts immediately to preserve system state
 */

#ifndef __TEST_H
#define __TEST_H

#include <lib/print.h>
#include <lib/utils.h>
#include <string.h>
#include <stdint.h>

/**
 * test_pass - Report a successful test.
 *
 * name: Name of the test or test suite.
 */
static inline void test_pass(const char *name)
{
    kprintf("[PASS] %s\n", name);
}

/**
 * test_fail - Report a failed test and halt the kernel.
 *
 * name: Name of the test or test suite.
 * msg:  Description of the failure.
 *
 * This function does not return.
 */
static inline void test_fail(const char *name, const char *msg)
{
    kprintf("[FAIL] %s: %s\n", name, msg);

    /* Halt immediately to preserve failure context */
    halt();
}

/*
 * TEST_ASSERT
 *
 * Fails if the condition evaluates to false.
 */
#define TEST_ASSERT(cond, name)     \
    do                              \
    {                               \
        if (!(cond))                \
            test_fail(name, #cond); \
    } while (0)

/*
 * TEST_ASSERT_EQ
 *
 * Fails if two values are not equal.
 */
#define TEST_ASSERT_EQ(a, b, name)        \
    do                                    \
    {                                     \
        if ((a) != (b))                   \
            test_fail(name, "not equal"); \
    } while (0)

/*
 * TEST_ASSERT_NEQ
 *
 * Fails if two values are equal.
 */
#define TEST_ASSERT_NEQ(a, b, name)                 \
    do                                              \
    {                                               \
        if ((a) == (b))                             \
            test_fail(name, "unexpected equality"); \
    } while (0)

/**
 * TEST_BEGIN - Mark the start of a test suite.
 */
#define TEST_BEGIN(name) \
    kprintf("Running %s...\n", name)

/**
 * TEST_END - Mark the successful completion of a test suite.
 */
#define TEST_END(name) \
    test_pass(name)

#endif /* __TEST_H */
