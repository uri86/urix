/*
 * Licensed under MIT License - URIX project.
 * test.h - Kernel test framework public interface.
 *
 * Responsibilities:
 *  - expose a single entry point for running all kernel tests
 */

#ifndef TEST_H
#define TEST_H

/**
 * run_all_tests - Execute all registered kernel tests.
 *
 * This function:
 *  - runs all test suites in a fixed order
 *  - halts the kernel on first failure
 */
void run_all_tests(void);

#endif /* TEST_H */
