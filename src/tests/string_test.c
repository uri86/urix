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

    /* memcpy */
    char src[] = "copy_me";
    char dest[16];
    memset(dest, 0, sizeof(dest));

    void *ret = memcpy(dest, src, 7);

    TEST_ASSERT_EQ(ret, dest, "memcpy return value");
    TEST_ASSERT_EQ(strncmp(dest, "copy_me", 7), 0, "memcpy content correct");
    TEST_ASSERT_EQ(dest[7], 0, "memcpy no overflow");

    /* memmove */
    char buf1[10] = "123456789";
    memmove(buf1 + 5, buf1, 3);
    TEST_ASSERT_EQ(strncmp(buf1, "123451239", 9), 0, "memmove non-overlap");
    char buf2[] = "12345";
    memmove(buf2, buf2 + 1, 3);
    TEST_ASSERT_EQ(strncmp(buf2, "23445", 5), 0, "memmove overlap left-shift");
    char buf3[] = "12345";
    memmove(buf3 + 1, buf3, 3);
    TEST_ASSERT_EQ(strncmp(buf3, "11235", 5), 0, "memmove overlap right-shift");

    /* memset */
    char buf4[8];
    void *ret2 = memset(buf4, 0x55, 8);
    TEST_ASSERT_EQ(ret2, buf4, "memset return value");
    TEST_ASSERT_EQ((unsigned char)buf4[0], 0x55, "memset byte 0");
    TEST_ASSERT_EQ((unsigned char)buf4[7], 0x55, "memset byte 7");
    memset(buf4 + 2, 0x00, 4);
    TEST_ASSERT_EQ((unsigned char)buf4[1], 0x55, "memset boundary guard pre");
    TEST_ASSERT_EQ((unsigned char)buf4[2], 0x00, "memset inner start");
    TEST_ASSERT_EQ((unsigned char)buf4[5], 0x00, "memset inner end");
    TEST_ASSERT_EQ((unsigned char)buf4[6], 0x55, "memset boundary guard post");

    TEST_END("string_tests");
}
