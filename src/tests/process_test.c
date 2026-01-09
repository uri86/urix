/*
 * Licensed under MIT License - URIX project.
 * process_test.c - Process Manager tests.
 */

#include <tests/__test.h>
#include <process/process.h>
#include <lib/string.h>

#define PROCESS_TEST_COUNT 50

/* Dummy entry point */
static void dummy_process(void)
{
    for (int i = 0; i < 10; i++) // Just yield a few times
    {
        process_yield();
    }
    process_exit(0); // Then exit cleanly
}

void process_tests(void)
{
    TEST_BEGIN("process_tests");

    int pids[PROCESS_TEST_COUNT];

    for (int i = 0; i < PROCESS_TEST_COUNT; i++)
    {
        char name[16];
        memset(name, 0, sizeof(name));
        strncpy(name, "ptest", 15);

        int pid = process_create(
            (uint64_t)dummy_process,
            name,
            PRIORITY_LOW,
            PROCESS_KERNEL);

        TEST_ASSERT(pid >= 0, "process_create failed");
        pids[i] = pid;
    }

    for (int i = 0; i < PROCESS_TEST_COUNT; i++)
    {
        process_t *p = process_get(pids[i]);
        TEST_ASSERT(p != NULL, "process_get returned NULL");
        TEST_ASSERT_EQ(p->pid, (uint32_t)pids[i], "pid mismatch");
    }

    for (int i = 0; i < PROCESS_TEST_COUNT; i++)
    {
        int rc = process_kill(pids[i]);
        TEST_ASSERT_EQ(rc, 0, "process_kill failed");
    }

    for (int i = 0; i < PROCESS_TEST_COUNT; i++)
    {
        process_t *p = process_get(pids[i]);
        TEST_ASSERT(p == NULL, "process not cleaned up");
    }

    TEST_END("process_tests");
}
