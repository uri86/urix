/*
 * Licensed under MIT License - URIX project.
 * blockdev.c - Block device registry and access.
 */

#include <fs/blockdev.h>
#include <string.h>
#include <lib/print.h>

static blockdev_t *device_list = NULL;

void blockdev_init(void)
{
    device_list = NULL;
}

int blockdev_register(blockdev_t *dev)
{
    if (!dev)
        return -1;
    if (blockdev_find(dev->name))
        return -1;

    dev->next = device_list;
    device_list = dev;
    return 0;
}

blockdev_t *blockdev_find(const char *name)
{
    blockdev_t *curr = device_list;
    while (curr)
    {
        if (strcmp(curr->name, name) == 0)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

blockdev_t *blockdev_get_list(void) { return device_list; }

int blockdev_read(blockdev_t *dev, uint64_t sector, void *buf, size_t count)
{
    if (!dev || !dev->ops || !dev->ops->read_block)
        return -1;
    if (sector + count > dev->sector_count)
        return -1;
    return dev->ops->read_block(dev, sector, buf, count);
}

int blockdev_write(blockdev_t *dev, uint64_t sector, const void *buf, size_t count)
{
    if (!dev || !dev->ops || !dev->ops->write_block)
        return -1;
    if (sector + count > dev->sector_count)
        return -1;
    return dev->ops->write_block(dev, sector, buf, count);
}