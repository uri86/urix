/*
 * Licensed under MIT License - URIX project.
 * test.h - Kernel test framework public interface.
 *
 * Responsibilities:
 *  - expose a single entry point for running all kernel tests
 *  - keep test internals hidden from the rest of the kernel
 *
 * Design:
 *  - Tests are executed sequentially during early kernel boot.
 *  - Any failure halts the kernel immediately.
 *
 * Notes:
 *  - This header is the ONLY test-related header included by kernel_main.
 *  - Individual test implementations are not visible outside the test subsystem.
 */

#ifndef TEST_H
#define TEST_H

/**
 * run_all_tests - Execute all registered kernel tests.
 *
 * This function:
 *  - runs all test suites in a fixed order
 *  - halts the kernel on first failure
 *  - prints a summary on success
 *
 * Must be called only after:
 *  - console output is initialized
 *  - paging is stable
 *  - PMM and VMM are initialized
 */
void run_all_tests(void);

#endif /* TEST_H */
