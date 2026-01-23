/*
 * Licensed under MIT License - URIX project.
 * ata.h - ATA/IDE disk driver interface.
 */

#ifndef ATA_H
#define ATA_H

#include <stdint.h>
#include <fs/blockdev.h>

/* ATA Commands */
#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY 0xEC

/* ATA Status bits */
#define ATA_SR_BSY 0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DF 0x20
#define ATA_SR_DRQ 0x08
#define ATA_SR_ERR 0x01

void ata_init(void);

#endif
