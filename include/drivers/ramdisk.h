/*
 * Licensed under MIT License - URIX project.
 * ramdisk.h - RAM disk driver interface.
 * Responsibilities:
 * - Declare RAM disk creation and management functions
 * - Provide interface for loading filesystem images
 */

#ifndef RAMDISK_H
#define RAMDISK_H

#include <stddef.h>
#include <fs/blockdev.h>

/**
 * ramdisk_create - Create a new RAM disk
 *
 * name: Device name
 * size_mb: Size in megabytes
 * out_dev: Pointer to receive the device structure
 *
 * Returns 0 on success, -1 on failure.
 */
int ramdisk_create(const char *name, size_t size_mb, blockdev_t **out_dev);

/**
 * ramdisk_load_image - Load a raw filesystem image into the RAM disk
 *
 * dev: Pointer to the RAM disk device
 * image: Source buffer
 * size: Size of source buffer
 *
 * Returns 0 on success, -1 on failure.
 */
int ramdisk_load_image(blockdev_t *dev, const void *image, size_t size);

#endif /* RAMDISK_H */