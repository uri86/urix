/*
 * Licensed under MIT License - URIX project.
 * test.c - Kernel test runner.
 *
 * Responsibilities:
 *  - invoke all kernel test suites in a deterministic order
 *  - provide a single integration point for kernel_main
 */

#include <tests/test.h>
#include <tests/__test.h>
#include <process/process.h>

/* Test suite declarations */
void string_tests(void);
void pmm_tests(void);
void vmm_tests(void);
void process_tests(void);
void vfs_tests(void);
void interrupt_tests(void);

void run_all_tests(void)
{
    kprintf("\n===== RUNNING KERNEL TESTS =====\n");

    /* Core utility tests */
    string_tests();

    /* Memory management tests */
    pmm_tests();
    vmm_tests();
    vfs_tests();
    process_tests();
    interrupt_tests();
    kprintf("\n===== ALL TESTS PASSED =====\n");
}
