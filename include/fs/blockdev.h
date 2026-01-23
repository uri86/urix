/*
 * Licensed under MIT License - URIX project.
 * blockdev.h - Block Device Abstraction Layer.
 * Responsibilities:
 * - Define generic block device structures
 * - Provide a registry for available storage devices
 * - Abstract read/write operations for filesystems
 */

#ifndef BLOCKDEV_H
#define BLOCKDEV_H

#include <stdint.h>
#include <stddef.h>

#define BLOCKDEV_MAX_NAME 32

#define BLOCKDEV_TYPE_DISK 1
#define BLOCKDEV_TYPE_RAMDISK 2
#define BLOCKDEV_TYPE_CDROM 3

typedef struct blockdev blockdev_t;

/*
 * Block Device Operations V-Table
 * Filesystems call these function pointers to interact with hardware.
 */
typedef struct blockdev_ops
{
    int (*read_block)(blockdev_t *dev, uint64_t sector, void *buf, size_t count);
    int (*write_block)(blockdev_t *dev, uint64_t sector, const void *buf, size_t count);
    int (*get_info)(blockdev_t *dev);
} blockdev_ops_t;

/*
 * Block Device Structure
 * Represents a physical or logical storage unit.
 */
struct blockdev
{
    char name[BLOCKDEV_MAX_NAME];
    uint8_t type;

    uint32_t sector_size;  /* Usually 512 bytes */
    uint64_t sector_count; /* Total LBA count */

    void *driver_data;   /* Private driver data (e.g., port base) */
    blockdev_ops_t *ops; /* Operations table */

    blockdev_t *next; /* Linked list next pointer */
};

/**
 * blockdev_init - Initialize the block device subsystem
 */
void blockdev_init(void);

/**
 * blockdev_register - Register a new device with the system
 */
int blockdev_register(blockdev_t *dev);

/**
 * blockdev_find - Find a device by name
 */
blockdev_t *blockdev_find(const char *name);

/**
 * blockdev_get_list - Retrieve the head of the device list
 */
blockdev_t *blockdev_get_list(void);

/**
 * blockdev_read - Read sectors from a device
 */
int blockdev_read(blockdev_t *dev, uint64_t sector, void *buf, size_t count);

/**
 * blockdev_write - Write sectors to a device
 */
int blockdev_write(blockdev_t *dev, uint64_t sector, const void *buf, size_t count);

#endif /* BLOCKDEV_H */