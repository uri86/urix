/*
 * Licensed under MIT License - URIX project.
 * vfs_manager.h - High-level VFS management.
 */
#ifndef VFS_MANAGER_H
#define VFS_MANAGER_H

/**
 * vfs_manager_init - Scans devices, formats if needed, and mounts root.
 * 
 * This function:
 * 1. Scans all available block devices
 * 2. Checks if they have a valid Ext2 filesystem
 * 3. Formats them if they don't
 * 4. Mounts the first device as root (/)
 * 5. Mounts additional devices as /devname
 * 
 * Returns: 0 on success, -1 if no root filesystem could be mounted
 */
int vfs_manager_init(void);

#endif /* VFS_MANAGER_H */