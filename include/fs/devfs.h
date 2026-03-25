/*
 * Licensed under MIT License - URIX project.
 * devfs.h - Device pseudo-filesystem interface.
 */

#ifndef DEVFS_H
#define DEVFS_H

/**
 * devfs_init - Initialize and register the devfs filesystem.
 * This should be called after VFS initializes.
 */
void devfs_init(void);

#endif /* DEVFS_H */
