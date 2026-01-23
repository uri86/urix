/*
 * Licensed under MIT License - URIX project.
 * vfs.c - VFS core implementation.
 */

#include <fs/vfs.h>
#include <memory/kmalloc.h>
#include <lib/string.h>
#include <lib/print.h>

static filesystem_t *filesystems = NULL;
static mount_t *root_mount = NULL;

void vfs_init(void)
{
    kprintf("\n=== VFS Init ===\n");
    filesystems = NULL;
    root_mount = NULL;
}

int vfs_register_fs(filesystem_t *fs)
{
    if (!fs)
        return -1;

    fs->next = filesystems;
    filesystems = fs;
    debug_kprintf("VFS: Registered filesystem '%s'\n", fs->name);
    return 0;
}

int vfs_mount(const char *fstype, const char *dev, const char *mountpoint)
{
    if (!fstype || !dev || !mountpoint)
        return -1;

    filesystem_t *fs = filesystems;
    while (fs && strcmp(fs->name, fstype) != 0)
        fs = fs->next;

    if (!fs)
    {
        debug_kprintf("VFS: Filesystem type '%s' not found\n", fstype);
        return -1;
    }

    mount_t *m = NULL;
    if (fs->mount(dev, &m) != 0)
    {
        kprintf("VFS: Failed to mount %s\n", dev);
        return -1;
    }

    m->mountpoint = kmalloc(strlen(mountpoint) + 1);
    if (!m->mountpoint)
        return -1;
    strcpy(m->mountpoint, mountpoint);

    if (strcmp(mountpoint, "/") == 0)
    {
        root_mount = m;
        debug_kprintf("VFS: Set root mount to %s\n", dev);
    }

    debug_kprintf("VFS: Mounted %s on %s (type: %s)\n", dev, mountpoint, fstype);
    return 0;
}

mount_t *vfs_get_root(void)
{
    return root_mount;
}

/* Helper function to split path into parent and filename */
static void split_path(const char *path, char *parent, char *name)
{
    const char *last_slash = NULL;
    const char *p = path;

    while (*p)
    {
        if (*p == '/')
            last_slash = p;
        p++;
    }

    if (!last_slash || last_slash == path)
    {
        strcpy(parent, "/");
        if (last_slash)
            strcpy(name, last_slash + 1);
        else
            strcpy(name, path);
    }
    else
    {
        size_t parent_len = last_slash - path;
        if (parent_len == 0)
        {
            strcpy(parent, "/");
        }
        else
        {
            memcpy(parent, path, parent_len);
            parent[parent_len] = '\0';
        }
        strcpy(name, last_slash + 1);
    }
}

static int vfs_resolve(const char *path, vnode_t **out)
{
    if (!path || !out)
        return -1;

    if (!root_mount)
    {
        debug_kprintf("VFS: No root mount\n");
        return -1;
    }

    vnode_t *curr = root_mount->root;

    if (strcmp(path, "/") == 0)
    {
        *out = curr;
        return 0;
    }

    if (path[0] == '/')
        path++;

    if (curr->ops && curr->ops->lookup)
    {
        if (curr->ops->lookup(curr, path, out) == 0)
            return 0;
    }

    return -1;
}

int vfs_open(const char *path, uint32_t flags, file_t **out)
{
    if (!path || !out)
        return -1;

    vnode_t *node = NULL;
    int resolved = vfs_resolve(path, &node);

    if (resolved != 0 && (flags & VFS_CREATE))
    {
        debug_kprintf("VFS: File '%s' not found, creating...\n", path);

        if (!root_mount || !root_mount->root)
        {
            debug_kprintf("VFS: No root directory\n");
            return -1;
        }

        vnode_t *parent = root_mount->root;
        const char *filename = path;
        if (path[0] == '/')
            filename = path + 1;

        if (strlen(filename) == 0)
        {
            debug_kprintf("VFS: Invalid filename\n");
            return -1;
        }

        if (!parent->ops || !parent->ops->create)
        {
            debug_kprintf("VFS: Filesystem doesn't support create\n");
            return -1;
        }

        if (parent->ops->create(parent, filename, &node) != 0)
        {
            debug_kprintf("VFS: Failed to create file '%s'\n", filename);
            return -1;
        }

        debug_kprintf("VFS: File '%s' created successfully\n", filename);
    }
    else if (resolved != 0)
    {
        debug_kprintf("VFS: File '%s' not found and CREATE flag not set\n", path);
        return -1;
    }
    else
    {
        debug_kprintf("VFS: Opened existing file '%s'\n", path);
    }

    file_t *f = kmalloc(sizeof(file_t));
    if (!f)
        return -1;

    f->vnode = node;
    f->flags = flags;

    /* Set initial offset based on flags */
    if (flags & VFS_APPEND)
    {
        /* For append mode, start at end of file */
        f->offset = node->size;
        debug_kprintf("VFS: Append mode - offset set to %lu\n", f->offset);
    }
    else if (flags & VFS_TRUNC)
    {
        /* For truncate mode, clear the file (would need truncate operation) */
        f->offset = 0;
    }
    else
    {
        /* Normal mode - start at beginning */
        f->offset = 0;
    }

    *out = f;

    return 0;
}

void vfs_close(file_t *f)
{
    if (f)
        kfree(f);
}

int vfs_read(file_t *f, void *buf, size_t size)
{
    if (!f || !f->vnode || !f->vnode->ops || !f->vnode->ops->read)
        return -1;

    int bytes = f->vnode->ops->read(f->vnode, buf, size, f->offset);
    if (bytes > 0)
        f->offset += bytes;

    return bytes;
}

int vfs_write(file_t *f, const void *buf, size_t size)
{
    if (!f || !f->vnode || !f->vnode->ops || !f->vnode->ops->write)
        return -1;

    int bytes = f->vnode->ops->write(f->vnode, buf, size, f->offset);
    if (bytes > 0)
        f->offset += bytes;

    return bytes;
}

int vfs_readdir(file_t *f, dirent_t *d)
{
    if (!f || !f->vnode || !f->vnode->ops || !f->vnode->ops->readdir)
        return -1;

    int ret = f->vnode->ops->readdir(f->vnode, d, f->offset);
    if (ret == 0)
        f->offset++;

    return ret;
}

int vfs_mkdir(const char *path)
{
    if (!path)
        return -1;

    if (!root_mount || !root_mount->root)
    {
        debug_kprintf("VFS: No root directory\n");
        return -1;
    }

    vnode_t *parent = root_mount->root;
    const char *dirname = path;
    if (path[0] == '/')
        dirname = path + 1;

    if (strlen(dirname) == 0)
    {
        debug_kprintf("VFS: Invalid directory name\n");
        return -1;
    }

    if (!parent->ops || !parent->ops->mkdir)
    {
        debug_kprintf("VFS: Filesystem doesn't support mkdir\n");
        return -1;
    }

    vnode_t *new_dir = NULL;
    if (parent->ops->mkdir(parent, dirname, &new_dir) != 0)
    {
        debug_kprintf("VFS: Failed to create directory '%s'\n", dirname);
        return -1;
    }

    if (new_dir)
        kfree(new_dir);

    debug_kprintf("VFS: Directory '%s' created successfully\n", dirname);
    return 0;
}

int vfs_unlink(const char *path)
{
    if (!path)
        return -1;

    if (!root_mount || !root_mount->root)
    {
        debug_kprintf("VFS: No root directory\n");
        return -1;
    }

    vnode_t *parent = root_mount->root;
    const char *filename = path;
    if (path[0] == '/')
        filename = path + 1;

    if (strlen(filename) == 0)
    {
        debug_kprintf("VFS: Invalid filename\n");
        return -1;
    }

    if (!parent->ops || !parent->ops->unlink)
    {
        debug_kprintf("VFS: Filesystem doesn't support unlink\n");
        return -1;
    }

    if (parent->ops->unlink(parent, filename) != 0)
    {
        debug_kprintf("VFS: Failed to unlink file '%s'\n", filename);
        return -1;
    }

    debug_kprintf("VFS: File '%s' unlinked successfully\n", filename);
    return 0;
}

int vfs_rmdir(const char *path)
{
    if (!path)
        return -1;

    if (!root_mount || !root_mount->root)
    {
        debug_kprintf("VFS: No root directory\n");
        return -1;
    }

    vnode_t *parent = root_mount->root;
    const char *dirname = path;
    if (path[0] == '/')
        dirname = path + 1;

    if (strlen(dirname) == 0)
    {
        debug_kprintf("VFS: Invalid directory name\n");
        return -1;
    }

    if (!parent->ops || !parent->ops->rmdir)
    {
        debug_kprintf("VFS: Filesystem doesn't support rmdir\n");
        return -1;
    }

    if (parent->ops->rmdir(parent, dirname) != 0)
    {
        debug_kprintf("VFS: Failed to remove directory '%s'\n", dirname);
        return -1;
    }

    debug_kprintf("VFS: Directory '%s' removed successfully\n", dirname);
    return 0;
}

int64_t vfs_seek(file_t *f, int64_t offset, int whence)
{
    if (!f || !f->vnode)
        return -1;

    int64_t new_offset;

    switch (whence)
    {
    case VFS_SEEK_SET:
        /* Absolute position */
        new_offset = offset;
        break;

    case VFS_SEEK_CUR:
        /* Relative to current position */
        new_offset = f->offset + offset;
        break;

    case VFS_SEEK_END:
        /* Relative to end of file */
        new_offset = f->vnode->size + offset;
        break;

    default:
        return -1;
    }

    /* Don't allow seeking before start of file */
    if (new_offset < 0)
        return -1;

    /* Allow seeking past end (will extend file on write) */
    f->offset = new_offset;
    return new_offset;
}