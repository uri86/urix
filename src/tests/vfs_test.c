/*
 * Licensed under MIT License - URIX project.
 * vfs_test.c - VFS and Ext2 filesystem tests.
 */
#include <tests/__test.h>
#include <fs/vfs.h>
#include <fs/blockdev.h>
#include <fs/ext2.h>
#include <drivers/ramdisk.h>
#include <lib/string.h>

/**
 * vfs_basic_tests - Test basic VFS and block device functionality
 */
void vfs_basic_tests(void)
{
    TEST_BEGIN("vfs_basic_tests");

    /* Initialize subsystems */
    blockdev_init();
    vfs_init();
    ext2_init();

    /* Create RAM disk */
    blockdev_t *ramdisk = NULL;
    TEST_ASSERT(ramdisk_create("test_disk", 16, &ramdisk) == 0,
                "ramdisk_create");
    TEST_ASSERT(ramdisk != NULL, "ramdisk not null");

    /* Find block device */
    blockdev_t *found = blockdev_find("test_disk");
    TEST_ASSERT(found == ramdisk, "blockdev_find");

    /* Read/write sectors */
    uint8_t write_buf[512];
    uint8_t read_buf[512];
    for (int i = 0; i < 512; i++)
        write_buf[i] = (uint8_t)i;

    TEST_ASSERT(blockdev_write(ramdisk, 0, write_buf, 1) == 0,
                "blockdev_write");
    TEST_ASSERT(blockdev_read(ramdisk, 0, read_buf, 1) == 0,
                "blockdev_read");

    /* Verify data */
    int match = 1;
    for (int i = 0; i < 512; i++)
    {
        if (read_buf[i] != write_buf[i])
        {
            match = 0;
            break;
        }
    }
    TEST_ASSERT(match, "read/write data match");

    /* Format the disk with Ext2 */
    TEST_ASSERT(ext2_create_filesystem(ramdisk) == 0,
                "ext2_create_filesystem");

    /* Mount the filesystem */
    TEST_ASSERT(vfs_mount("ext2", "test_disk", "/") == 0,
                "vfs_mount");

    /* Verify root mount exists */
    mount_t *root = vfs_get_root();
    TEST_ASSERT(root != NULL, "vfs_get_root");

    TEST_END("vfs_basic_tests");
}

/**
 * vfs_file_tests - Test file creation, writing, and reading
 */
void vfs_file_tests(void)
{
    TEST_BEGIN("vfs_file_tests");

    /* Create a new file */
    file_t *f = NULL;
    TEST_ASSERT(vfs_open("/test.txt", VFS_CREATE | VFS_WRITE, &f) == 0,
                "create file");
    TEST_ASSERT(f != NULL, "file handle not null");

    /* Write data to file */
    const char *test_data = "Hello, URIX filesystem!";
    int bytes_written = vfs_write(f, test_data, strlen(test_data));
    TEST_ASSERT(bytes_written == (int)strlen(test_data),
                "write correct number of bytes");
    vfs_close(f);

    /* Read the file back */
    TEST_ASSERT(vfs_open("/test.txt", VFS_READ, &f) == 0,
                "open existing file");

    char read_buf[128];
    memset(read_buf, 0, sizeof(read_buf));
    int bytes_read = vfs_read(f, read_buf, sizeof(read_buf) - 1);
    TEST_ASSERT(bytes_read == (int)strlen(test_data),
                "read correct number of bytes");
    TEST_ASSERT(strcmp(read_buf, test_data) == 0,
                "read data matches written data");
    vfs_close(f);

    /* Append to file */
    TEST_ASSERT(vfs_open("/test.txt", VFS_WRITE | VFS_APPEND, &f) == 0,
                "reopen file for append");

    const char *append_data = " More data!";
    vfs_write(f, append_data, strlen(append_data));
    vfs_close(f);

    /* Verify appended data */
    TEST_ASSERT(vfs_open("/test.txt", VFS_READ, &f) == 0,
                "open file after append");

    memset(read_buf, 0, sizeof(read_buf));
    bytes_read = vfs_read(f, read_buf, sizeof(read_buf) - 1);

    char expected[128];
    strcpy(expected, test_data);
    strcat(expected, append_data);

    TEST_ASSERT(strcmp(read_buf, expected) == 0,
                "appended data correct");
    vfs_close(f);

    /* Create multiple files */
    TEST_ASSERT(vfs_open("/file1.txt", VFS_CREATE | VFS_WRITE, &f) == 0,
                "create file1");
    vfs_write(f, "File 1", 6);
    vfs_close(f);

    TEST_ASSERT(vfs_open("/file2.txt", VFS_CREATE | VFS_WRITE, &f) == 0,
                "create file2");
    vfs_write(f, "File 2", 6);
    vfs_close(f);

    TEST_ASSERT(vfs_open("/file3.txt", VFS_CREATE | VFS_WRITE, &f) == 0,
                "create file3");
    vfs_write(f, "File 3", 6);
    vfs_close(f);

    TEST_END("vfs_file_tests");
}

/**
 * vfs_directory_tests - Test directory operations
 */
void vfs_directory_tests(void)
{
    TEST_BEGIN("vfs_directory_tests");

    /* Create a directory */
    TEST_ASSERT(vfs_mkdir("/mydir") == 0, "create directory");

    /* Verify directory exists by opening it */
    file_t *f = NULL;
    TEST_ASSERT(vfs_open("/mydir", VFS_READ, &f) == 0,
                "open directory");
    TEST_ASSERT(f->vnode->type == VFS_DIR, "verify it's a directory");
    vfs_close(f);

    /* Create nested directories */
    TEST_ASSERT(vfs_mkdir("/docs") == 0, "create docs directory");
    TEST_ASSERT(vfs_mkdir("/data") == 0, "create data directory");

    /* Try to create duplicate directory */
    int result = vfs_mkdir("/mydir");
    TEST_ASSERT(result != 0, "duplicate directory creation fails");

    /* Test 5: Create files in directories would require path parsing
     * For now, we test that directories are properly created */

    TEST_END("vfs_directory_tests");
}

/**
 * vfs_readdir_tests - Test directory listing
 */
void vfs_readdir_tests(void)
{
    TEST_BEGIN("vfs_readdir_tests");

    /* Open root directory */
    file_t *f = NULL;
    TEST_ASSERT(vfs_open("/", VFS_READ, &f) == 0, "open root directory");

    /* Read directory entries */
    dirent_t entry;
    int entry_count = 0;
    int found_test_txt = 0;
    int found_mydir = 0;

    kprintf("\nDirectory listing of /:\n");
    while (vfs_readdir(f, &entry) == 0)
    {
        kprintf("  - %s (inode: %lu, type: %u)\n",
                entry.name, entry.inode, entry.type);
        entry_count++;

        if (strcmp(entry.name, "test.txt") == 0)
            found_test_txt = 1;
        if (strcmp(entry.name, "mydir") == 0)
            found_mydir = 1;
    }

    TEST_ASSERT(entry_count >= 2, "found multiple entries");
    TEST_ASSERT(found_test_txt, "found test.txt in listing");
    TEST_ASSERT(found_mydir, "found mydir in listing");

    vfs_close(f);

    TEST_END("vfs_readdir_tests");
}

/**
 * vfs_delete_tests - Test file and directory deletion
 */
void vfs_delete_tests(void)
{
    TEST_BEGIN("vfs_delete_tests");

    /* Delete a file */
    TEST_ASSERT(vfs_unlink("/file1.txt") == 0, "delete file1.txt");

    /* Verify file is gone */
    file_t *f = NULL;
    int result = vfs_open("/file1.txt", VFS_READ, &f);
    TEST_ASSERT(result != 0, "deleted file no longer exists");

    /* Delete remaining test files */
    TEST_ASSERT(vfs_unlink("/file2.txt") == 0, "delete file2.txt");
    TEST_ASSERT(vfs_unlink("/file3.txt") == 0, "delete file3.txt");

    /* Try to delete non-existent file (should fail) */
    result = vfs_unlink("/nonexistent.txt");
    TEST_ASSERT(result != 0, "deleting non-existent file fails");

    /* Try to delete directory with unlink (should fail) */
    result = vfs_unlink("/mydir");
    TEST_ASSERT(result != 0, "cannot unlink directory");

    /* Remove empty directory */
    TEST_ASSERT(vfs_rmdir("/docs") == 0, "remove empty directory");

    /* Verify directory is gone */
    result = vfs_open("/docs", VFS_READ, &f);
    TEST_ASSERT(result != 0, "deleted directory no longer exists");

    TEST_END("vfs_delete_tests");
}

/**
 * vfs_persistence_test - Test data persistence
 */
void vfs_persistence_test(void)
{
    TEST_BEGIN("vfs_persistence_test");

    file_t *f = NULL;
    const char *persist_file = "/persist.dat";
    const char *persist_data = "This data should persist across remounts!";

    /* Create and write persistence file */
    TEST_ASSERT(vfs_open(persist_file, VFS_CREATE | VFS_WRITE, &f) == 0,
                "create persistence file");

    int written = vfs_write(f, persist_data, strlen(persist_data));
    TEST_ASSERT(written == (int)strlen(persist_data), "write persistence data");
    vfs_close(f);

    /* Read back immediately */
    TEST_ASSERT(vfs_open(persist_file, VFS_READ, &f) == 0,
                "reopen persistence file");

    char read_buf[128];
    memset(read_buf, 0, sizeof(read_buf));
    int read_bytes = vfs_read(f, read_buf, sizeof(read_buf) - 1);

    TEST_ASSERT(read_bytes == (int)strlen(persist_data),
                "read correct amount of data");
    TEST_ASSERT(strcmp(read_buf, persist_data) == 0,
                "persistence data matches");
    vfs_close(f);

    TEST_END("vfs_persistence_test");
}

/**
 * vfs_stress_test - Stress test the filesystem
 */
void vfs_stress_test(void)
{
    TEST_BEGIN("vfs_stress_test");

    file_t *f = NULL;

    /* Create many small files */
    kprintf("\nCreating 10 test files...\n");
    for (int i = 0; i < 10; i++)
    {
        char filename[32];
        char content[64];

        // Simple integer-to-string conversion
        filename[0] = '/';
        filename[1] = 't';
        filename[2] = 'e';
        filename[3] = 's';
        filename[4] = 't';
        filename[5] = '0' + (i / 10);
        filename[6] = '0' + (i % 10);
        filename[7] = '.';
        filename[8] = 't';
        filename[9] = 'x';
        filename[10] = 't';
        filename[11] = '\0';

        strcpy(content, "File number ");
        content[12] = '0' + (i / 10);
        content[13] = '0' + (i % 10);
        content[14] = '\0';

        if (vfs_open(filename, VFS_CREATE | VFS_WRITE, &f) == 0)
        {
            vfs_write(f, content, strlen(content));
            vfs_close(f);
        }
    }

    /* Verify all files exist and contain correct data */
    int all_correct = 1;
    for (int i = 0; i < 10; i++)
    {
        char filename[32];
        char expected[64];
        char read_buf[64];

        filename[0] = '/';
        filename[1] = 't';
        filename[2] = 'e';
        filename[3] = 's';
        filename[4] = 't';
        filename[5] = '0' + (i / 10);
        filename[6] = '0' + (i % 10);
        filename[7] = '.';
        filename[8] = 't';
        filename[9] = 'x';
        filename[10] = 't';
        filename[11] = '\0';

        strcpy(expected, "File number ");
        expected[12] = '0' + (i / 10);
        expected[13] = '0' + (i % 10);
        expected[14] = '\0';

        if (vfs_open(filename, VFS_READ, &f) == 0)
        {
            memset(read_buf, 0, sizeof(read_buf));
            vfs_read(f, read_buf, sizeof(read_buf) - 1);
            vfs_close(f);

            if (strcmp(read_buf, expected) != 0)
                all_correct = 0;
        }
        else
        {
            all_correct = 0;
        }
    }

    TEST_ASSERT(all_correct, "all stress test files correct");

    /* Clean up stress test files */
    for (int i = 0; i < 10; i++)
    {
        char filename[32];

        filename[0] = '/';
        filename[1] = 't';
        filename[2] = 'e';
        filename[3] = 's';
        filename[4] = 't';
        filename[5] = '0' + (i / 10);
        filename[6] = '0' + (i % 10);
        filename[7] = '.';
        filename[8] = 't';
        filename[9] = 'x';
        filename[10] = 't';
        filename[11] = '\0';

        vfs_unlink(filename);
    }

    TEST_END("vfs_stress_test");
}

/**
 * vfs_demo - Main VFS demonstration and test suite
 */
void vfs_demo(void)
{
    kprintf("\n");
    kprintf("====================================\n");
    kprintf("   VFS and Ext2 Test Suite\n");
    kprintf("====================================\n");
    kprintf("\n");

    /* Run all test suites */
    vfs_basic_tests();
    vfs_file_tests();
    vfs_directory_tests();
    vfs_readdir_tests();
    vfs_delete_tests();
    vfs_persistence_test();
    vfs_stress_test();

    kprintf("\n");
    kprintf("========================================\n");
    kprintf("   All VFS Tests Complete!\n");
    kprintf("========================================\n");
    kprintf("\n");
}

/**
 * vfs_tests - Alias for backward compatibility
 */
void vfs_tests(void)
{
    vfs_demo();
}