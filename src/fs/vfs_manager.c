/*
 * Licensed under MIT License - URIX project.
 * vfs_manager.c - VFS manager implementation.
 */
#include <fs/vfs_manager.h>
#include <fs/vfs.h>
#include <fs/blockdev.h>
#include <fs/ext2.h>
#include <lib/print.h>
#include <string.h>
#include <memory/kmalloc.h>

int vfs_manager_init(void)
{
    kprintf("\n=== VFS Manager: Auto-Scan ===\n");

    blockdev_t *dev = blockdev_get_list();
    int mounted_root = 0;

    while (dev)
    {
        kprintf("VFS Manager: Probing %s... ", dev->name);

        /* Read Superblock candidate (Sector 2, Offset 1024 bytes) */
        uint8_t *buf = kmalloc(1024);
        if (!buf)
        {
            kprintf("[ERR] Memory allocation failed\n");
            dev = dev->next;
            continue;
        }

        if (blockdev_read(dev, 2, buf, 2) != 0)
        {
            kprintf("[ERR] Read failed\n");
            kfree(buf);
            dev = dev->next;
            continue;
        }

        /* Check Magic Number at offset 56 of the Superblock struct */
        uint16_t magic = *(uint16_t *)(buf + 56);
        kfree(buf);

        if (magic == EXT2_MAGIC)
        {
            kprintf("[FOUND] Valid Ext2 Filesystem detected.\n");
        }
        else
        {
            kprintf("[EMPTY] No filesystem. Formatting %s...\n", dev->name);
            if (ext2_create_filesystem(dev) != 0)
            {
                kprintf("  [ERR] Failed to format %s\n", dev->name);
                dev = dev->next;
                continue;
            }
            kprintf("  [OK] Formatting complete.\n");
        }

        /* Mount the filesystem */
        if (!mounted_root)
        {
            /* First valid drive becomes Root (/) */
            if (vfs_mount("ext2", dev->name, "/") == 0)
            {
                kprintf("  Mounted %s as ROOT (/)\n", dev->name);
                mounted_root = 1;
            }
            else
            {
                kprintf("  [ERR] Failed to mount %s as root\n", dev->name);
            }
        }
        else
        {
            /* Mount others as /devname */
            char path[32];
            path[0] = '/';
            strncpy(path + 1, dev->name, 30);
            path[31] = '\0';

            if (vfs_mount("ext2", dev->name, path) == 0)
            {
                kprintf("  Mounted %s as %s\n", dev->name, path);
            }
            else
            {
                kprintf("  [WARN] Failed to mount %s\n", dev->name);
            }
        }

        dev = dev->next;
    }

    if (!mounted_root)
    {
        kprintf("\nVFS Manager: FATAL - No Root FS found.\n");
        return -1;
    }

    kprintf("=== VFS Manager: Initialization Complete ===\n\n");
    return 0;
}