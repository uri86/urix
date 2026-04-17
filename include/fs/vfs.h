/*
 * Licensed under MIT License - URIX project.
 * vfs.h - Virtual File System Interface.
 * Responsibilities:
 * - Abstract differences between filesystems
 * - Manage mounts, nodes (inodes), and file handles
 */
#ifndef VFS_H
#define VFS_H
#include <stdint.h>
#include <stddef.h>

/* Node Types */
#define VFS_FILE 1
#define VFS_DIR 2

/* Open Flags */
#define VFS_READ 0x01
#define VFS_WRITE 0x02
#define VFS_CREATE 0x04
#define VFS_TRUNC 0x08
#define VFS_APPEND 0x10

#define VFS_MAX_NAME 256

typedef struct vnode vnode_t;
typedef struct mount mount_t;
typedef struct filesystem filesystem_t;

/*
 * Directory Entry structure
 */
typedef struct dirent
{
    uint64_t inode;
    char name[VFS_MAX_NAME];
    uint8_t type;
} dirent_t;

/*
 * VFS Operations
 * Filesystems implement these hooks.
 */
struct vfs_ops
{
    int (*read)(vnode_t *node, void *buf, size_t size, uint64_t offset);
    int (*write)(vnode_t *node, const void *buf, size_t size, uint64_t offset);
    int (*readdir)(vnode_t *node, dirent_t *dirent, uint64_t index);
    int (*lookup)(vnode_t *dir, const char *name, vnode_t **result);
    int (*create)(vnode_t *dir, const char *name, vnode_t **result);
    int (*mkdir)(vnode_t *dir, const char *name, vnode_t **result);
    int (*unlink)(vnode_t *dir, const char *name);
    int (*rmdir)(vnode_t *dir, const char *name);
    int (*truncate)(vnode_t *node);
    void (*release)(vnode_t *node);
};

/*
 * V-Node (Virtual Node)
 * Represents an active file/directory in memory.
 */
struct vnode
{
    uint64_t inode;
    uint8_t type;
    uint64_t size;
    uint32_t refcount;
    mount_t *mount; // The filesystem this node belongs to
    mount_t *mounted_here; // A filesystem mounted over this directory
    void *fs_data; /* Filesystem specific data */
    struct vfs_ops *ops;
};

/*
 * Filesystem Driver Registration
 */
struct filesystem
{
    char name[32];
    int (*mount)(const char *dev, mount_t **mnt);
    int (*unmount)(mount_t *mnt);
    filesystem_t *next;
};

/*
 * Mount Point
 */
struct mount
{
    filesystem_t *fs;
    vnode_t *root;
    void *fs_data; /* FS specific mount data */
    char *mountpoint;
    mount_t *next;
    
    // enables to cross mount boundaries
    uint64_t parent_inode; 
    struct mount *parent_mount;
};

/*
 * Open File Handle
 */
typedef struct file
{
    vnode_t *vnode;
    uint64_t offset;
    uint32_t flags;
} file_t;

/* Core VFS Functions */
void vfs_init(void);
int vfs_register_fs(filesystem_t *fs);
int vfs_mount(const char *fstype, const char *dev, const char *mountpoint);
mount_t *vfs_get_root(void);

/* File Operations */
int vfs_open(const char *path, uint32_t flags, file_t **out);
void vfs_retain_file(file_t *f);
void vfs_close(file_t *f);
int vfs_read(file_t *f, void *buf, size_t size);
int vfs_write(file_t *f, const void *buf, size_t size);
int vfs_readdir(file_t *f, dirent_t *dirent);
int64_t vfs_seek(file_t *f, int64_t offset, int whence);

/* Seek whence values */
#define VFS_SEEK_SET 0 /* Absolute position */
#define VFS_SEEK_CUR 1 /* Relative to current */
#define VFS_SEEK_END 2 /* Relative to end */

/* Directory Operations */
int vfs_resolve_path(const char *path, char *out_path);
int vfs_mkdir(const char *path);
int vfs_unlink(const char *path);
int vfs_rmdir(const char *path);

#endif /* VFS_H */