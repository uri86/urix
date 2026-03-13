/*
 * Licensed under MIT License - URIX project.
 * vfs.c - VFS core implementation.
 */

#include <fs/vfs.h>
#include <memory/kmalloc.h>
#include <string.h>
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
        if (root_mount->root)
            root_mount->root->refcount = 1;
        debug_kprintf("VFS: Set root mount to %s\n", dev);
    }

    debug_kprintf("VFS: Mounted %s on %s (type: %s)\n", dev, mountpoint, fstype);
    return 0;
}

mount_t *vfs_get_root(void)
{
    return root_mount;
}


static void vfs_vnode_get(vnode_t *node)
{
    if (!node)
        return;
    node->refcount++;
    debug_kprintf("VFS: vnode_get inode=%lu refcount=%u\n", node->inode, node->refcount);
}


static void vfs_vnode_put(vnode_t *node)
{
    if (!node)
        return;

    if (node->refcount > 0)
        node->refcount--;

    debug_kprintf("VFS: vnode_put inode=%lu refcount=%u\n", node->inode, node->refcount);

    /* Don't free the root vnode */
    if (node == root_mount->root)
        return;

    /* Only free when no references remain */
    if (node->refcount == 0)
    {
        debug_kprintf("VFS: Freeing vnode inode=%lu\n", node->inode);
        if (node->ops && node->ops->release)
            node->ops->release(node);
    }
}

static int vfs_resolve(const char *path, vnode_t **out)
{
    if (!path || !out)
        return -1;

    if (!root_mount || !root_mount->root)
    {
        debug_kprintf("VFS: No root mount\n");
        return -1;
    }

    /* Initialize output to NULL */
    *out = NULL;

    /* Handle root directory */
    if (strcmp(path, "/") == 0)
    {
        *out = root_mount->root;
        vfs_vnode_get(*out);
        return 0;
    }

    vnode_t *current = root_mount->root;
    vfs_vnode_get(current);

    const char *p = path;
    char component[VFS_MAX_NAME];

    /* Skip leading slash */
    if (*p == '/')
        p++;

    /* Parse each component */
    while (1)
    {
        /* Extract next component */
        size_t i = 0;
        while (*p && *p != '/' && i < VFS_MAX_NAME - 1)
        {
            component[i++] = *p++;
        }
        component[i] = '\0';

        if (i == 0)
            break;

        /* Look up component in current directory */
        if (!current || !current->ops || !current->ops->lookup)
        {
            debug_kprintf("VFS: No lookup operation for vnode\n");
            vfs_vnode_put(current);
            return -1;
        }

        vnode_t *next = NULL;
        if (current->ops->lookup(current, component, &next) != 0)
        {
            debug_kprintf("VFS: Component '%s' not found in path '%s'\n", component, path);
            vfs_vnode_put(current);
            return -1;
        }

        /* Check if lookup returned NULL */
        if (!next)
        {
            debug_kprintf("VFS: lookup returned NULL for component '%s'\n", component);
            vfs_vnode_put(current);
            return -1;
        }

        vfs_vnode_put(current);
        current = next;

        /* Skip trailing slashes */
        while (*p == '/')
            p++;
        
        if (*p == '\0')
            break;
    }

    *out = current;
    return 0;
}

/*
 * vfs_resolve_parent - Resolve parent directory and get filename
 */
static int vfs_resolve_parent(const char *path, vnode_t **parent, char *filename, size_t filename_size)
{
    if (!path || !parent || !filename || filename_size == 0)
        return -1;

    if (!root_mount || !root_mount->root)
        return -1;

    /* Initialize output to NULL */
    *parent = NULL;

    /* Find last slash */
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
        *parent = root_mount->root;
        vfs_vnode_get(*parent);

        const char *name = (path[0] == '/') ? path + 1 : path;
        strncpy(filename, name, filename_size);
        filename[filename_size - 1] = '\0';

        return 0;
    }

    /* Build parent path */
    size_t parent_len = last_slash - path;
    char parent_path[256];

    if (parent_len == 0)
    {
        /* Parent is root */
        strcpy(parent_path, "/");
    }
    else
    {
        if (parent_len >= sizeof(parent_path))
            parent_len = sizeof(parent_path) - 1;
        memcpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';
    }

    /* Copy filename */
    strncpy(filename, last_slash + 1, filename_size);
    filename[filename_size - 1] = '\0';
    int result = vfs_resolve(parent_path, parent);

    if (result != 0 || !*parent)
    {
        debug_kprintf("VFS: Failed to resolve parent path '%s'\n", parent_path);
        return -1;
    }

    return 0;
}

int vfs_open(const char *path, uint32_t flags, file_t **out)
{
    if (!path || !out)
        return -1;

    vnode_t *node = NULL;
    int resolved = vfs_resolve(path, &node);

    if (resolved == 0) {
        // Prevent writing to directories
        if ((flags & VFS_WRITE) && node->type == VFS_DIR) {
            vfs_vnode_put(node);
            return -1;
        }
    }

    if (resolved != 0 && (flags & VFS_CREATE))
    {
        debug_kprintf("VFS: File '%s' not found, creating...\n", path);

        vnode_t *parent = NULL;
        char filename[VFS_MAX_NAME];

        if (vfs_resolve_parent(path, &parent, filename, sizeof(filename)) != 0)
        {
            debug_kprintf("VFS: Failed to resolve parent directory for '%s'\n", path);
            return -1;
        }

        if (!parent)
        {
            debug_kprintf("VFS: Parent directory is NULL for '%s'\n", path);
            return -1;
        }

        if (strlen(filename) == 0)
        {
            debug_kprintf("VFS: Invalid filename\n");
            vfs_vnode_put(parent);
            return -1;
        }

        if (!parent->ops || !parent->ops->create)
        {
            debug_kprintf("VFS: Filesystem doesn't support create\n");
            vfs_vnode_put(parent);
            return -1;
        }

        if (parent->ops->create(parent, filename, &node) != 0)
        {
            debug_kprintf("VFS: Failed to create file '%s'\n", filename);
            vfs_vnode_put(parent);
            return -1;
        }

        vfs_vnode_put(parent);

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

    if (!node)
    {
        debug_kprintf("VFS: Failed to get vnode for '%s'\n", path);
        return -1;
    }

    file_t *f = kmalloc(sizeof(file_t));
    if (!f)
    {
        vfs_vnode_put(node);
        return -1;
    }

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
        f->offset = 0;
    }
    else
    {
        f->offset = 0;
    }

    *out = f;
    return 0;
}

void vfs_retain_file(file_t *f)
{
    if (!f || !f->vnode)
        return;

    vfs_vnode_get(f->vnode);
}

void vfs_close(file_t *f)
{
    if (!f)
        return;

    if (f->vnode)
    {
        vfs_vnode_put(f->vnode);
    }

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

    vnode_t *parent = NULL;
    char dirname[VFS_MAX_NAME];

    if (vfs_resolve_parent(path, &parent, dirname, sizeof(dirname)) != 0)
    {
        debug_kprintf("VFS: Failed to resolve parent directory for '%s'\n", path);
        return -1;
    }

    if (!parent)
    {
        debug_kprintf("VFS: Parent directory is NULL for '%s'\n", path);
        return -1;
    }

    if (strlen(dirname) == 0)
    {
        debug_kprintf("VFS: Invalid directory name\n");
        vfs_vnode_put(parent);
        return -1;
    }

    if (!parent->ops || !parent->ops->mkdir)
    {
        debug_kprintf("VFS: Filesystem doesn't support mkdir\n");
        vfs_vnode_put(parent);
        return -1;
    }

    vnode_t *new_dir = NULL;
    if (parent->ops->mkdir(parent, dirname, &new_dir) != 0)
    {
        debug_kprintf("VFS: Failed to create directory '%s'\n", dirname);
        vfs_vnode_put(parent);
        return -1;
    }

    if (new_dir)
        vfs_vnode_put(new_dir);

    vfs_vnode_put(parent);

    debug_kprintf("VFS: Directory '%s' created successfully\n", dirname);
    return 0;
}

int vfs_mkdir_p(const char *path)
{
    if (!path)
        return -1;

    if (strcmp(path, "/") == 0)
        return 0;

    char temp_path[256];
    strncpy(temp_path, path, sizeof(temp_path) - 1);
    temp_path[sizeof(temp_path) - 1] = '\0';

    char *p = temp_path;
    if (*p == '/')
        p++;

    while (*p)
    {
        char *next_slash = strchr(p, '/');

        if (next_slash)
            *next_slash = '\0';

        char full_path[256];
        full_path[0] = '/';
        strcpy(full_path + 1, temp_path);

        vnode_t *test = NULL;
        if (vfs_resolve(full_path, &test) != 0)
        {
            debug_kprintf("VFS: Creating intermediate directory '%s'\n", full_path);
            if (vfs_mkdir(full_path) != 0)
            {
                debug_kprintf("VFS: Failed to create intermediate directory '%s'\n", full_path);
                return -1;
            }
        }
        else
        {
            /* Directory exists, release the reference */
            vfs_vnode_put(test);
        }

        if (!next_slash)
            break;

        *next_slash = '/';
        p = next_slash + 1;
    }

    return 0;
}

int vfs_unlink(const char *path)
{
    if (!path)
        return -1;

    vnode_t *parent = NULL;
    char filename[VFS_MAX_NAME];

    if (vfs_resolve_parent(path, &parent, filename, sizeof(filename)) != 0)
    {
        debug_kprintf("VFS: Failed to resolve parent directory for '%s'\n", path);
        return -1;
    }

    if (!parent)
    {
        debug_kprintf("VFS: Parent directory is NULL for '%s'\n", path);
        return -1;
    }

    if (strlen(filename) == 0)
    {
        debug_kprintf("VFS: Invalid filename\n");
        vfs_vnode_put(parent);
        return -1;
    }

    if (!parent->ops || !parent->ops->unlink)
    {
        debug_kprintf("VFS: Filesystem doesn't support unlink\n");
        vfs_vnode_put(parent);
        return -1;
    }

    if (parent->ops->unlink(parent, filename) != 0)
    {
        debug_kprintf("VFS: Failed to unlink file '%s'\n", filename);
        vfs_vnode_put(parent);
        return -1;
    }

    vfs_vnode_put(parent);
    debug_kprintf("VFS: File '%s' unlinked successfully\n", filename);
    return 0;
}

int vfs_rmdir(const char *path)
{
    if (!path)
        return -1;

    vnode_t *parent = NULL;
    char dirname[VFS_MAX_NAME];

    if (vfs_resolve_parent(path, &parent, dirname, sizeof(dirname)) != 0)
    {
        debug_kprintf("VFS: Failed to resolve parent directory for '%s'\n", path);
        return -1;
    }

    if (!parent)
    {
        debug_kprintf("VFS: Parent directory is NULL for '%s'\n", path);
        return -1;
    }

    if (strlen(dirname) == 0)
    {
        debug_kprintf("VFS: Invalid directory name\n");
        vfs_vnode_put(parent);
        return -1;
    }

    if (!parent->ops || !parent->ops->rmdir)
    {
        debug_kprintf("VFS: Filesystem doesn't support rmdir\n");
        vfs_vnode_put(parent);
        return -1;
    }

    if (parent->ops->rmdir(parent, dirname) != 0)
    {
        debug_kprintf("VFS: Failed to remove directory '%s'\n", dirname);
        vfs_vnode_put(parent);
        return -1;
    }

    vfs_vnode_put(parent);
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

    f->offset = new_offset;
    return new_offset;
}