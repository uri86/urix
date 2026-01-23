/*
 * Licensed under MIT License - URIX project.
 * ramdisk.c - RAM disk driver implementation.
 */

#include <drivers/ramdisk.h>
#include <memory/kmalloc.h>
#include <lib/string.h>
#include <lib/print.h>

typedef struct
{
    uint8_t *buffer;
    size_t size;
} ramdisk_data_t;

static int ramdisk_read(blockdev_t *dev, uint64_t sector, void *buf, size_t count)
{
    ramdisk_data_t *data = (ramdisk_data_t *)dev->driver_data;
    uint64_t offset = sector * dev->sector_size;
    size_t size = count * dev->sector_size;

    if (offset + size > data->size)
        return -1;
    memcpy(buf, data->buffer + offset, size);
    return 0;
}

static int ramdisk_write(blockdev_t *dev, uint64_t sector, const void *buf, size_t count)
{
    ramdisk_data_t *data = (ramdisk_data_t *)dev->driver_data;
    uint64_t offset = sector * dev->sector_size;
    size_t size = count * dev->sector_size;

    if (offset + size > data->size)
        return -1;
    memcpy(data->buffer + offset, buf, size);
    return 0;
}

static blockdev_ops_t ramdisk_ops = {
    .read_block = ramdisk_read,
    .write_block = ramdisk_write};

int ramdisk_create(const char *name, size_t size_mb, blockdev_t **out_dev)
{
    kprintf("RAMDisk: Creating %s (%d MB)\n", name, size_mb);
    blockdev_t *dev = kmalloc(sizeof(blockdev_t));
    ramdisk_data_t *data = kmalloc(sizeof(ramdisk_data_t));

    if (!dev || !data)
        return -1;

    data->size = size_mb * 1024 * 1024;
    data->buffer = kmalloc(data->size);
    if (!data->buffer)
    {
        kfree(dev);
        kfree(data);
        return -1;
    }
    memset(data->buffer, 0, data->size);

    memset(dev, 0, sizeof(blockdev_t));
    strncpy(dev->name, name, BLOCKDEV_MAX_NAME);
    dev->type = BLOCKDEV_TYPE_RAMDISK;
    dev->sector_size = 512;
    dev->sector_count = data->size / 512;
    dev->ops = &ramdisk_ops;
    dev->driver_data = data;

    blockdev_register(dev);
    if (out_dev)
        *out_dev = dev;
    return 0;
}

int ramdisk_load_image(blockdev_t *dev, const void *image, size_t size)
{
    ramdisk_data_t *data = (ramdisk_data_t *)dev->driver_data;
    if (size > data->size)
        return -1;
    memcpy(data->buffer, image, size);
    return 0;
}