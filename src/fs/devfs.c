/*
 * Licensed under MIT License - URIX project.
 * devfs.c - Device Virtual File System (/dev).
 */

#include <fs/devfs.h>
#include <fs/vfs.h>
#include <memory/kmalloc.h>
#include <string.h>
#include <lib/print.h>
#include <drivers/vga.h>
#include <drivers/keyboard.h>
#include <fs/blockdev.h>

#define DEVFS_DEVICES_COUNT 7
static const char *dev_entries[DEVFS_DEVICES_COUNT] = {
    "null", "zero", "console", "keyboard", "tty", "stdin", "stdout"};

/* dev/null ops */
static int devfs_null_read(vnode_t *node, void *buf, size_t size, uint64_t offset)
{
    (void)node;
    (void)buf;
    (void)offset;
    return 0; // EOF immediately
}
static int devfs_null_write(vnode_t *node, const void *buf, size_t size, uint64_t offset)
{
    (void)node;
    (void)buf;
    (void)offset;
    return size; // Consumed successfully
}
static struct vfs_ops devfs_null_ops = {
    .read = devfs_null_read,
    .write = devfs_null_write,
    .readdir = NULL,
    .lookup = NULL,
    .create = NULL,
    .mkdir = NULL,
    .unlink = NULL,
    .rmdir = NULL,
    .truncate = NULL,
    .release = NULL};

/* dev/zero ops */
static int devfs_zero_read(vnode_t *node, void *buf, size_t size, uint64_t offset)
{
    (void)node;
    (void)offset;
    memset(buf, 0, size);
    return size;
}
static int devfs_zero_write(vnode_t *node, const void *buf, size_t size, uint64_t offset)
{
    (void)node;
    (void)buf;
    (void)offset;
    return size; // Consume silently
}
static struct vfs_ops devfs_zero_ops = {
    .read = devfs_zero_read,
    .write = devfs_zero_write,
    .readdir = NULL,
    .lookup = NULL,
    .create = NULL,
    .mkdir = NULL,
    .unlink = NULL,
    .rmdir = NULL,
    .truncate = NULL,
    .release = NULL};

/* dev/console & dev/tty ops */
static int devfs_console_read(vnode_t *node, void *buf, size_t size, uint64_t offset)
{
    (void)node;
    (void)offset;
    if (size == 0)
        return 0;

    char *cbuf = (char *)buf;
    size_t i = 0;
    while (i < size)
    {
        cbuf[i] = keyboard_getchar_blocking();
        console_putchar(cbuf[i]); // Echo back
        i++;
        // Break on newline
        if (cbuf[i - 1] == '\n' || cbuf[i - 1] == '\r')
        {
            if (cbuf[i - 1] == '\r')
                cbuf[i - 1] = '\n';
            break;
        }
    }
    return i;
}
static int devfs_console_write(vnode_t *node, const void *buf, size_t size, uint64_t offset)
{
    (void)node;
    (void)offset;
    console_write((const char *)buf, size);
    return size;
}
static struct vfs_ops devfs_console_ops = {
    .read = devfs_console_read,
    .write = devfs_console_write,
    .readdir = NULL,
    .lookup = NULL,
    .create = NULL,
    .mkdir = NULL,
    .unlink = NULL,
    .rmdir = NULL,
    .truncate = NULL,
    .release = NULL};

/* dev/keyboard ops */
static int devfs_keyboard_read(vnode_t *node, void *buf, size_t size, uint64_t offset)
{
    (void)node;
    (void)offset;
    if (size == 0)
        return 0;
    char *cbuf = (char *)buf;
    cbuf[0] = keyboard_getchar_blocking();
    return 1;
}
static struct vfs_ops devfs_keyboard_ops = {
    .read = devfs_keyboard_read,
    .write = NULL,
    .readdir = NULL,
    .lookup = NULL,
    .create = NULL,
    .mkdir = NULL,
    .unlink = NULL,
    .rmdir = NULL,
    .truncate = NULL,
    .release = NULL};

static void devfs_release(vnode_t *node)
{
    kfree(node);
}

/* devfs root ops */
static int devfs_root_readdir(vnode_t *node, dirent_t *dirent, uint64_t index)
{
    (void)node;
    if (index < DEVFS_DEVICES_COUNT)
    {
        strncpy(dirent->name, dev_entries[index], VFS_MAX_NAME - 1);
        dirent->name[VFS_MAX_NAME - 1] = '\0';
        dirent->type = VFS_FILE;
        dirent->inode = index + 1;
        return 0;
    }

    /* Dynamically add block devices after the static array */
    uint64_t blk_index = index - DEVFS_DEVICES_COUNT;
    blockdev_t *blk = blockdev_get_list();
    for (uint64_t i = 0; blk && i < blk_index; i++)
    {
        blk = blk->next;
    }

    if (blk)
    {
        strncpy(dirent->name, blk->name, VFS_MAX_NAME - 1);
        dirent->name[VFS_MAX_NAME - 1] = '\0';
        dirent->type = VFS_FILE;
        dirent->inode = index + 1;
        return 0;
    }

    return -1; // EOF
}

static int devfs_root_lookup(vnode_t *dir, const char *name, vnode_t **result)
{
    (void)dir;
    struct vfs_ops *ops = NULL;

    if (strcmp(name, "null") == 0)
        ops = &devfs_null_ops;
    else if (strcmp(name, "zero") == 0)
        ops = &devfs_zero_ops;
    else if (strcmp(name, "console") == 0 || strcmp(name, "tty") == 0 ||
             strcmp(name, "stdin") == 0 || strcmp(name, "stdout") == 0)
    {
        ops = &devfs_console_ops;
    }
    else if (strcmp(name, "keyboard") == 0)
        ops = &devfs_keyboard_ops;
    else
    {
        // Checking for a block device name
        blockdev_t *blk = blockdev_get_list();
        while (blk)
        {
            if (strcmp(name, blk->name) == 0)
            {
                ops = &devfs_null_ops; // Mock it as null
                break;
            }
            blk = blk->next;
        }
    }

    if (!ops)
        return -1;

    vnode_t *node = kmalloc(sizeof(vnode_t));
    if (!node)
        return -1;
    memset(node, 0, sizeof(vnode_t));

    node->type = VFS_FILE;
    node->refcount = 1;
    node->ops = ops;

    /* Attach release hook so it gets freed when closed */
    if (!ops->release)
    {
        ops->release = devfs_release;
    }

    *result = node;
    return 0;
}

static struct vfs_ops devfs_root_ops = {
    .read = NULL, 
    .write = NULL, 
    .readdir = devfs_root_readdir, 
    .lookup = devfs_root_lookup, 
    .create = NULL, 
    .mkdir = NULL, 
    .unlink = NULL, 
    .rmdir = NULL, 
    .truncate = NULL, 
    .release = NULL
};

/* devfs interface */
static int devfs_mount_cb(const char *dev, mount_t **mnt)
{
    (void)dev;
    mount_t *m = kmalloc(sizeof(mount_t));
    if (!m)
        return -1;
    memset(m, 0, sizeof(mount_t));

    vnode_t *root = kmalloc(sizeof(vnode_t));
    if (!root)
    {
        kfree(m);
        return -1;
    }
    memset(root, 0, sizeof(vnode_t));

    root->inode = 0;
    root->type = VFS_DIR;
    root->refcount = 1; // Kept alive by mount point
    root->ops = &devfs_root_ops;
    root->mount = m;

    m->root = root;
    *mnt = m;
    return 0;
}

static filesystem_t devfs_fs = {
    .name = "devfs",
    .mount = devfs_mount_cb,
    .unmount = NULL,
    .next = NULL};

void devfs_init(void)
{
    kprintf("VFS: Registering devfs...\n");
    vfs_register_fs(&devfs_fs);

    // Create /dev dir in root fs if it doesn't exist
    vfs_mkdir("/dev");

    // Mount devfs to /dev
    if (vfs_mount("devfs", "none", "/dev") == 0)
    {
        kprintf("VFS: Mounted devfs to /dev successfully\n");
    }
    else
    {
        kprintf("VFS: completely failed to mount devfs to /dev\n");
    }
}
