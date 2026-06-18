/*
 * Licensed under MIT License - URIX project.
 * ata.c - ATA/IDE PIO driver
 */

#include <drivers/ata.h>
#include <fs/blockdev.h>
#include <memory/kmalloc.h>
#include <string.h>
#include <lib/print.h>
#include <ports.h>

/* ATA registers (offsets) */
#define ATA_REG_DATA 0x00
#define ATA_REG_ERROR 0x01
#define ATA_REG_SECCOUNT0 0x02
#define ATA_REG_LBA0 0x03
#define ATA_REG_LBA1 0x04
#define ATA_REG_LBA2 0x05
#define ATA_REG_HDDEVSEL 0x06
#define ATA_REG_STATUS 0x07
#define ATA_REG_COMMAND 0x07

#define ATA_PRIMARY_IO 0x1F0
#define ATA_SECONDARY_IO 0x170
#define ATA_PRIMARY_CTRL 0x3F6
#define ATA_SECONDARY_CTRL 0x376

typedef struct
{
    uint16_t base;
    uint16_t ctrl;
    uint8_t slave;
} ata_device_t;

/* 400ns delay */
static inline void ata_io_wait(uint16_t ctrl)
{
    inb(ctrl);
    inb(ctrl);
    inb(ctrl);
    inb(ctrl);
}

/* Wait for BSY=0. Returns 0 on success, -1 on timeout/error. */
static int ata_wait_not_busy(uint16_t base)
{
    for (int i = 0; i < 100000; i++)
    {
        uint8_t st = inb(base + ATA_REG_STATUS);
        if (st == 0xFF)
            return -1; // Floating bus
        if (!(st & ATA_SR_BSY))
            return 0;
    }
    return -1;
}

/* Wait for DRQ=1 and BSY=0 */
static int ata_wait_drq(uint16_t base)
{
    for (int i = 0; i < 100000; i++)
    {
        uint8_t st = inb(base + ATA_REG_STATUS);
        if (st == 0xFF)
            return -1;
        if (st & ATA_SR_ERR)
            return -1;
        if (st & ATA_SR_DF)
            return -1;
        if (!(st & ATA_SR_BSY) && (st & ATA_SR_DRQ))
            return 0;
    }
    return -1;
}

static void ata_select(ata_device_t *ata, uint32_t lba)
{
    // LBA28: 0xE0 | Slave Bit | Top 4 bits of LBA
    outb(ata->base + ATA_REG_HDDEVSEL, 0xE0 | (ata->slave << 4) | ((lba >> 24) & 0x0F));
    ata_io_wait(ata->ctrl);
}

static int ata_read_block(blockdev_t *dev, uint64_t sector, void *buf, size_t count)
{
    ata_device_t *ata = (ata_device_t *)dev->driver_data;
    uint16_t *dst = (uint16_t *)buf;

    if (sector + count > 0x10000000)
        return -1; // 28-bit limit check

    for (size_t i = 0; i < count; i++)
    {
        uint32_t lba = sector + i;

        if (ata_wait_not_busy(ata->base) != 0)
            return -1;

        ata_select(ata, lba);

        outb(ata->base + ATA_REG_SECCOUNT0, 1);
        outb(ata->base + ATA_REG_LBA0, (uint8_t)lba);
        outb(ata->base + ATA_REG_LBA1, (uint8_t)(lba >> 8));
        outb(ata->base + ATA_REG_LBA2, (uint8_t)(lba >> 16));
        outb(ata->base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

        if (ata_wait_drq(ata->base) != 0)
            return -1;

        for (int w = 0; w < 256; w++)
            dst[i * 256 + w] = inw(ata->base + ATA_REG_DATA);

        // Read status to acknowledge interrupt/clear state
        inb(ata->base + ATA_REG_STATUS);
    }
    return 0;
}

static int ata_write_block(blockdev_t *dev, uint64_t sector, const void *buf, size_t count)
{
    ata_device_t *ata = (ata_device_t *)dev->driver_data;
    const uint16_t *src = (const uint16_t *)buf;

    if (sector + count > 0x10000000)
        return -1;

    // Write each sector
    for (size_t i = 0; i < count; i++)
    {
        uint32_t lba = sector + i;

        if (ata_wait_not_busy(ata->base) != 0)
            return -1;

        ata_select(ata, lba);

        outb(ata->base + ATA_REG_SECCOUNT0, 1);
        outb(ata->base + ATA_REG_LBA0, (uint8_t)lba);
        outb(ata->base + ATA_REG_LBA1, (uint8_t)(lba >> 8));
        outb(ata->base + ATA_REG_LBA2, (uint8_t)(lba >> 16));
        outb(ata->base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

        ata_io_wait(ata->ctrl);
        if (ata_wait_not_busy(ata->base) != 0)
            return -1;

        // Now check if DRQ is set
        if (!(inb(ata->base + ATA_REG_STATUS) & ATA_SR_DRQ))
            return -1;

        for (int w = 0; w < 256; w++)
            outw(ata->base + ATA_REG_DATA, src[i * 256 + w]);
    }

    if (ata_wait_not_busy(ata->base) != 0)
        return -1;

    outb(ata->base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    if (ata_wait_not_busy(ata->base) != 0)
        return -1;

    return 0;
}

static blockdev_ops_t ata_ops = {
    .read_block = ata_read_block,
    .write_block = ata_write_block};


static void ata_probe(uint16_t base, uint16_t ctrl, int slave, const char *name)
{
    // Check for floating bus
    if (inb(base + ATA_REG_STATUS) == 0xFF)
        return;

    outb(base + ATA_REG_HDDEVSEL, 0xA0 | (slave << 4));
    ata_io_wait(ctrl);

    outb(base + ATA_REG_SECCOUNT0, 0);
    outb(base + ATA_REG_LBA0, 0);
    outb(base + ATA_REG_LBA1, 0);
    outb(base + ATA_REG_LBA2, 0);
    outb(base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_io_wait(ctrl);

    uint8_t st = inb(base + ATA_REG_STATUS);
    if (st == 0)
        return;

    // Wait for BSY to clear
    int retry = 10000;
    while ((inb(base + ATA_REG_STATUS) & ATA_SR_BSY) && retry-- > 0)
        ;

    // Check Error
    st = inb(base + ATA_REG_STATUS);
    if (st & ATA_SR_ERR)
        return;

    // Wait for DRQ
    retry = 10000;
    while (!(inb(base + ATA_REG_STATUS) & ATA_SR_DRQ) && retry-- > 0)
        ;
    if (retry <= 0)
        return;

    uint16_t id[256];
    for (int i = 0; i < 256; i++)
        id[i] = inw(base + ATA_REG_DATA);

    uint32_t sectors = id[60] | (id[61] << 16);
    if (sectors == 0)
        return;

    blockdev_t *dev = kmalloc(sizeof(blockdev_t));
    ata_device_t *priv = kmalloc(sizeof(ata_device_t));
    memset(dev, 0, sizeof(blockdev_t));

    // Copy the device name and set up the block device structure
    strncpy(dev->name, name, BLOCKDEV_MAX_NAME);
    dev->type = BLOCKDEV_TYPE_DISK;
    dev->sector_size = 512;
    dev->sector_count = sectors;
    dev->ops = &ata_ops;

    priv->base = base;
    priv->ctrl = ctrl;
    priv->slave = slave;
    dev->driver_data = priv;

    blockdev_register(dev);
    kprintf("ATA: %s detected (%u MB)\n", name, sectors / 2048);
}

void ata_init(void)
{
    kprintf("Initializing ATA...\n");
    ata_probe(ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, 0, "hda");
    ata_probe(ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, 1, "hdb");
    ata_probe(ATA_SECONDARY_IO, ATA_SECONDARY_CTRL, 0, "hdc");
    ata_probe(ATA_SECONDARY_IO, ATA_SECONDARY_CTRL, 1, "hdd");
}