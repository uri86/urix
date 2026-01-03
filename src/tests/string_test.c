/*
 * Licensed under MIT License - URIX project.
 * string_test.c - Unit tests for kernel string utilities.
 *
 * Responsibilities:
 *  - verify correctness of basic string operations
 *  - validate edge cases (NULL handling, padding, termination)
 *
 * Notes:
 *  - These tests are non-destructive and order-independent.
 *  - They must pass before any higher-level subsystems are tested.
 */

#include <tests/__test.h>
#include <lib/string.h>

void string_tests(void)
{
    TEST_BEGIN("string_tests");

    /* strlen */
    TEST_ASSERT_EQ(strlen(""), 0, "strlen empty");
    TEST_ASSERT_EQ(strlen("a"), 1, "strlen single char");
    TEST_ASSERT_EQ(strlen("hello"), 5, "strlen word");

    /* strcmp */
    TEST_ASSERT_EQ(strcmp("a", "a"), 0, "strcmp equal");
    TEST_ASSERT(strcmp("a", "b") < 0, "strcmp less");
    TEST_ASSERT(strcmp("b", "a") > 0, "strcmp greater");
    TEST_ASSERT_EQ(strcmp(NULL, "a"), -1, "strcmp NULL");

    /* strncmp */
    TEST_ASSERT_EQ(strncmp("hello", "helium", 3), 0, "strncmp prefix");
    TEST_ASSERT_NEQ(strncmp("hello", "helium", 5), 0, "strncmp mismatch");

    /* strncpy */
    char buf[8];
    memset(buf, 0xAA, sizeof(buf));
    strncpy(buf, "hi", 5);

    TEST_ASSERT_EQ(buf[0], 'h', "strncpy char 0");
    TEST_ASSERT_EQ(buf[1], 'i', "strncpy char 1");
    TEST_ASSERT_EQ(buf[2], '\0', "strncpy null padding");

    TEST_END("string_tests");
}
