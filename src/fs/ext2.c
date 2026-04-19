/*
 * Licensed under MIT License - URIX project.
 * ext2.c - EXT2 file system implementation.
 */

#include <fs/ext2.h>
#include <memory/kmalloc.h>
#include <string.h>
#include <lib/print.h>

/* Forward Declarations */
static int ext2_mount(const char *dev, mount_t **mnt);
static int ext2_lookup(vnode_t *dir, const char *name, vnode_t **res);
static int ext2_create(vnode_t *dir, const char *name, vnode_t **res);
static int ext2_mkdir(vnode_t *dir, const char *name, vnode_t **res);
static int ext2_unlink(vnode_t *dir, const char *name);
static int ext2_rmdir(vnode_t *dir, const char *name);
static int ext2_read(vnode_t *node, void *buf, size_t size, uint64_t offset);
static int ext2_write(vnode_t *node, const void *buf, size_t size, uint64_t offset);
static int ext2_readdir(vnode_t *node, dirent_t *dirent, uint64_t index);
static int ext2_truncate(vnode_t *node);
static void ext2_release(vnode_t *node);
static int ext2_read_inode(ext2_mount_t *m, uint32_t ino, ext2_inode_t *out);
static void ext2_print_stats(mount_t *mnt);
static void ext2_print_blocks(vnode_t *node);
static void ext2_print_dir(vnode_t *node);
static void ext2_trace_path(mount_t *mnt, const char *path);

static int read_fs_block(ext2_mount_t *m, uint32_t block, void *data);

static filesystem_t ext2_fs = {
    .name = "ext2",
    .mount = ext2_mount,
    .unmount = NULL,
    .print_stats = ext2_print_stats,
    .trace_path = ext2_trace_path,
    .next = NULL};

static struct vfs_ops ext2_ops = {
    .read = ext2_read,
    .write = ext2_write,
    .readdir = ext2_readdir,
    .lookup = ext2_lookup,
    .create = ext2_create,
    .mkdir = ext2_mkdir,
    .unlink = ext2_unlink,
    .rmdir = ext2_rmdir,
    .truncate = ext2_truncate,
    .release = ext2_release,
    .print_blocks = ext2_print_blocks,
    .print_dir = ext2_print_dir};

void ext2_init(void)
{
    vfs_register_fs(&ext2_fs);
}

static void ext2_print_stats(mount_t *mnt)
{
    ext2_mount_t *m = (ext2_mount_t *)mnt->fs_data;
    kprintf("\n=== Ext2 Superblock Stats ===\n");
    kprintf("Inodes count: %u\n", m->sb.s_inodes_count);
    kprintf("Blocks count: %u\n", m->sb.s_blocks_count);
    kprintf("Free inodes:  %u\n", m->sb.s_free_inodes_count);
    kprintf("Free blocks:  %u\n", m->sb.s_free_blocks_count);
    kprintf("First data block: %u\n", m->sb.s_first_data_block);
    kprintf("Block size: %u\n", m->block_size);
    kprintf("Blocks per group: %u\n", m->sb.s_blocks_per_group);
    kprintf("Inodes per group: %u\n", m->sb.s_inodes_per_group);
    kprintf("Magic: 0x%x\n\n", m->sb.s_magic);
}

static void ext2_print_blocks(vnode_t *node)
{
    ext2_mount_t *m = (ext2_mount_t *)node->mount->fs_data;
    ext2_inode_t inode;
    if (ext2_read_inode(m, node->inode, &inode) != 0) return;
    
    kprintf("\n=== Ext2 File Blocks (inode %lu) ===\n", node->inode);
    kprintf("Size: %u bytes\n", inode.i_size);
    kprintf("Blocks used: %u\n", inode.i_blocks);
    
    kprintf("Direct blocks: ");
    for (int i = 0; i < 12; i++) {
        if (inode.i_block[i] != 0) kprintf("%u ", inode.i_block[i]);
    }
    kprintf("\n");
    
    if (inode.i_block[12] != 0) {
        kprintf("Single Indirect block: %u\n", inode.i_block[12]);
        uint8_t *ind_buf = kmalloc(m->block_size);
        if (ind_buf) {
            read_fs_block(m, inode.i_block[12], ind_buf);
            kprintf("  -> ");
            uint32_t *ptrs = (uint32_t *)ind_buf;
            uint32_t count = m->block_size / sizeof(uint32_t);
            for (uint32_t i = 0; i < count; i++) {
                if (ptrs[i] != 0) kprintf("%u ", ptrs[i]);
            }
            kprintf("\n");
            kfree(ind_buf);
        }
    }
    kprintf("\n");
}

static void ext2_print_dir(vnode_t *node)
{
    if (node->type != VFS_DIR) {
        kprintf("\nExt2: Not a directory (inode %lu)\n", node->inode);
        return;
    }
    
    ext2_mount_t *m = (ext2_mount_t *)node->mount->fs_data;
    ext2_inode_t inode;
    if (ext2_read_inode(m, node->inode, &inode) != 0) return;
    
    kprintf("\n=== Ext2 Directory Entries (inode %lu) ===\n", node->inode);
    uint8_t *buf = kmalloc(m->block_size);
    if (!buf) return;
    
    for (int i = 0; i < 12; i++) {
        if (inode.i_block[i] == 0) break;
        
        read_fs_block(m, inode.i_block[i], buf);
        uint32_t offset = 0;
        kprintf("Block %u:\n", inode.i_block[i]);
        
        while (offset < m->block_size) {
            ext2_dirent_t *d = (ext2_dirent_t *)(buf + offset);
            if (d->rec_len == 0) {
                kprintf("  [offset %u] BREAK (rec_len=0)\n", offset);
                break;
            }
            
            char name_buf[256];
            uint8_t len = (d->name_len < 255) ? d->name_len : 255;
            memcpy(name_buf, d->name, len);
            name_buf[len] = '\0';
            
            kprintf("  [offset %u] ino: %u | rec_len: %u | type: %u | name: '%s'\n", 
                    offset, d->inode, d->rec_len, d->file_type, name_buf);
                    
            offset += d->rec_len;
        }
    }
    kfree(buf);
    kprintf("\n");
}

static void ext2_trace_path(mount_t *mnt, const char *path)
{
    ext2_mount_t *m = (ext2_mount_t *)mnt->fs_data;

    /* --- Superblock header --- */
    kprintf("\n=== VFS Path Trace: '%s' ===\n", path);
    kprintf("Filesystem : ext2  Magic: 0x%x  Block size: %u\n",
            m->sb.s_magic, m->block_size);
    kprintf("Inodes: %u total, %u free  |  Blocks: %u total, %u free\n\n",
            m->sb.s_inodes_count, m->sb.s_free_inodes_count,
            m->sb.s_blocks_count, m->sb.s_free_blocks_count);

    /* Start at root inode (inode 2) */
    uint32_t cur_ino = EXT2_ROOT_INO;
    kprintf("[/]  inode %u  (root)\n", cur_ino);

    /* Walk each path component */
    char comp[256];
    const char *p = path;
    while (*p == '/') p++;       /* skip leading slash */

    uint8_t *buf = kmalloc(m->block_size);
    if (!buf) { kprintf("Ext2 trace: out of memory\n"); return; }

    while (*p)
    {
        /* Extract component */
        size_t ci = 0;
        while (*p && *p != '/' && ci < 255)
            comp[ci++] = *p++;
        comp[ci] = '\0';
        while (*p == '/') p++;

        /* Read current inode */
        ext2_inode_t inode;
        if (ext2_read_inode(m, cur_ino, &inode) != 0) {
            kprintf("  ERROR: could not read inode %u\n", cur_ino);
            break;
        }

        kprintf("  Searching for '%s' in inode %u  (mode: 0x%x, size: %u)\n",
                comp, cur_ino, inode.i_mode, inode.i_size);

        /* Scan direct blocks for the entry */
        int found = 0;
        for (int bi = 0; bi < 12 && !found; bi++)
        {
            if (inode.i_block[bi] == 0) break;

            kprintf("    Block[%d] = %u  -> scanning dirents...\n",
                    bi, inode.i_block[bi]);
            read_fs_block(m, inode.i_block[bi], buf);

            uint32_t offset = 0;
            while (offset < m->block_size)
            {
                ext2_dirent_t *d = (ext2_dirent_t *)(buf + offset);
                if (d->rec_len == 0) break;

                if (d->inode && d->name_len == ci &&
                    strncmp(d->name, comp, d->name_len) == 0)
                {
                    kprintf("    MATCH  inode %u  type %u  rec_len %u  name '%s'\n",
                            d->inode, d->file_type, d->rec_len, comp);
                    cur_ino = d->inode;
                    found = 1;
                    break;
                }
                offset += d->rec_len;
            }
        }

        if (!found) {
            kprintf("  NOT FOUND: '%s'\n", comp);
            kfree(buf);
            return;
        }

        kprintf("  -> entered inode %u  ('%s')\n\n", cur_ino, comp);
    }

    /* --- Destination inode details --- */
    ext2_inode_t dest;
    if (ext2_read_inode(m, cur_ino, &dest) == 0)
    {
        const char *kind = (dest.i_mode & 0x4000) ? "DIR" :
                           (dest.i_mode & 0x8000) ? "FILE" : "OTHER";
        kprintf("=== Destination: inode %u  type: %s  size: %u ===\n",
                cur_ino, kind, dest.i_size);
        kprintf("Direct blocks: ");
        for (int i = 0; i < 12; i++)
            if (dest.i_block[i]) kprintf("%u ", dest.i_block[i]);
        kprintf("\n");
        if (dest.i_block[12]) {
            kprintf("Indirect block: %u  -> ",  dest.i_block[12]);
            read_fs_block(m, dest.i_block[12], buf);
            uint32_t *ptrs = (uint32_t *)buf;
            uint32_t cnt   = m->block_size / sizeof(uint32_t);
            for (uint32_t i = 0; i < cnt; i++)
                if (ptrs[i]) kprintf("%u ", ptrs[i]);
            kprintf("\n");
        }
    }
    kprintf("\n");
    kfree(buf);
}

/* Read a filesystem block using block numbers */
static int read_fs_block(ext2_mount_t *m, uint32_t block, void *data)
{
    uint32_t sectors_per_block = m->block_size / 512;
    uint64_t start_sector = (uint64_t)block * sectors_per_block;
    return blockdev_read(m->dev, start_sector, data, sectors_per_block);
}

/* Write a filesystem block using block numbers */
static int write_fs_block(ext2_mount_t *m, uint32_t block, void *data)
{
    uint32_t sectors_per_block = m->block_size / 512;
    uint64_t start_sector = (uint64_t)block * sectors_per_block;
    return blockdev_write(m->dev, start_sector, data, sectors_per_block);
}

/* Read superblock from standard location (block 1 = sectors 2-3) */
static int read_superblock(blockdev_t *dev, ext2_superblock_t *sb)
{
    uint8_t *buf = kmalloc(1024);
    if (!buf)
        return -1;

    if (blockdev_read(dev, 2, buf, 2) != 0)
    {
        kfree(buf);
        return -1;
    }

    memcpy(sb, buf, sizeof(ext2_superblock_t));
    kfree(buf);
    return 0;
}

/* Write superblock to standard location */
static int write_superblock(blockdev_t *dev, ext2_superblock_t *sb)
{
    uint8_t *buf = kmalloc(1024);
    if (!buf)
        return -1;

    memset(buf, 0, 1024);
    memcpy(buf, sb, sizeof(ext2_superblock_t));

    int ret = blockdev_write(dev, 2, buf, 2);
    kfree(buf);
    return ret;
}

static int ext2_read_inode(ext2_mount_t *m, uint32_t ino, ext2_inode_t *out)
{
    if (ino == 0)
        return -1;
    uint32_t group = (ino - 1) / m->sb.s_inodes_per_group;
    uint32_t idx = (ino - 1) % m->sb.s_inodes_per_group;
    uint32_t blk = m->bgdt[group].bg_inode_table + (idx * m->inode_size) / m->block_size;
    uint32_t off = (idx * m->inode_size) % m->block_size;

    uint8_t *buf = kmalloc(m->block_size);
    if (!buf)
        return -1;
    read_fs_block(m, blk, buf);
    memcpy(out, buf + off, sizeof(ext2_inode_t));
    kfree(buf);
    return 0;
}

static int ext2_write_inode(ext2_mount_t *m, uint32_t ino, ext2_inode_t *in)
{
    if (ino == 0)
        return -1;
    // calculate the inode place
    uint32_t group = (ino - 1) / m->sb.s_inodes_per_group;
    uint32_t idx = (ino - 1) % m->sb.s_inodes_per_group;
    uint32_t blk = m->bgdt[group].bg_inode_table + (idx * m->inode_size) / m->block_size;
    uint32_t off = (idx * m->inode_size) % m->block_size;

    uint8_t *buf = kmalloc(m->block_size);
    if (!buf)
        return -1;
    read_fs_block(m, blk, buf);
    memcpy(buf + off, in, sizeof(ext2_inode_t));
    write_fs_block(m, blk, buf);
    kfree(buf);
    return 0;
}

/* Update superblock and BGDT on disk */
static void ext2_sync_metadata(ext2_mount_t *m)
{
    write_superblock(m->dev, &m->sb);
    write_fs_block(m, 2, m->bgdt);
}

static int find_free_bit(uint8_t *map, uint32_t size_bytes)
{
    for (uint32_t i = 0; i < size_bytes; i++)
    {
        if (map[i] == 0xFF)
            continue;               // continues forward if the byte is full
        for (int j = 0; j < 8; j++) // searches for the the empty bit
        {
            if (!((map[i] >> j) & 1))
                return i * 8 + j; // returns the index of the bit
        }
    }
    return -1; // return -1 if no free bit was found
}

/* Both functions below assume that map is not null. */
static void set_bit(uint8_t *map, int bit) { map[bit / 8] |= (1 << (bit % 8)); }
static void clear_bit(uint8_t *map, int bit) { map[bit / 8] &= ~(1 << (bit % 8)); }

static int alloc_bitmap_bit(ext2_mount_t *m, uint32_t bitmap_block)
{
    uint8_t *buf = kmalloc(m->block_size);
    if (!buf) return -1;
    if (read_fs_block(m, bitmap_block, buf) != 0) { kfree(buf); return -1; }
    
    int bit = find_free_bit(buf, m->block_size);
    if (bit == -1) { kfree(buf); return -1; }
    
    set_bit(buf, bit);
    write_fs_block(m, bitmap_block, buf);
    kfree(buf);
    return bit;
}

static void free_bitmap_bit(ext2_mount_t *m, uint32_t bitmap_block, int bit)
{
    uint8_t *buf = kmalloc(m->block_size);
    if (!buf) return;
    if (read_fs_block(m, bitmap_block, buf) != 0) { kfree(buf); return; }
    
    clear_bit(buf, bit);
    write_fs_block(m, bitmap_block, buf);
    kfree(buf);
}

static uint32_t alloc_inode(ext2_mount_t *m)
{
    int bit = alloc_bitmap_bit(m, m->bgdt[0].bg_inode_bitmap);
    if (bit == -1) return 0;
    
    m->sb.s_free_inodes_count--;
    m->bgdt[0].bg_free_inodes_count--;
    ext2_sync_metadata(m);
    return bit + 1;
}

static void free_inode(ext2_mount_t *m, uint32_t ino)
{
    if (ino == 0) return;
    free_bitmap_bit(m, m->bgdt[0].bg_inode_bitmap, ino - 1);
    
    m->sb.s_free_inodes_count++;
    m->bgdt[0].bg_free_inodes_count++;
    ext2_sync_metadata(m);
}

static uint32_t alloc_block(ext2_mount_t *m)
{
    int bit = alloc_bitmap_bit(m, m->bgdt[0].bg_block_bitmap);
    if (bit == -1) return 0;
    
    m->sb.s_free_blocks_count--;
    m->bgdt[0].bg_free_blocks_count--;
    ext2_sync_metadata(m);
    return m->sb.s_first_data_block + bit;
}

static void free_block(ext2_mount_t *m, uint32_t block)
{
    if (block < m->sb.s_first_data_block) return;
    free_bitmap_bit(m, m->bgdt[0].bg_block_bitmap, block - m->sb.s_first_data_block);
    
    m->sb.s_free_blocks_count++;
    m->bgdt[0].bg_free_blocks_count++;
    ext2_sync_metadata(m);
}

/**
 * This function performs two passes:
 * 1. Scans for duplicates to ensure the name doesn't already exist.
 * 2. Scans for a free slot. If a slot (gap) is found by splitting an existing
 * entry (that has extra padding in rec_len), it inserts there.
 * Otherwise, it allocates a new block for the directory.
 *
 * m - Mounted filesystem context.
 * dir_ino - The inode number of the directory to modify.
 * new_ino - The inode number of the new file/directory being added.
 * name - The name of the new entry.
 * type - The type of entry (1=File, 2=Directory).
 *  returns an int, 0 on success, -1 on failure.
 */
static int dir_add_entry(ext2_mount_t *m, uint32_t dir_ino, uint32_t new_ino, const char *name, uint8_t type)
{
    ext2_inode_t dir_inode;
    if (ext2_read_inode(m, dir_ino, &dir_inode) != 0)
        return -1;

    uint8_t *buf = kmalloc(m->block_size);
    if (!buf)
        return -1;

    // Scan existing blocks to ensure the filename isn't already used.
    for (int i = 0; i < 12; i++)
    {
        if (dir_inode.i_block[i] == 0)
            break;

        read_fs_block(m, dir_inode.i_block[i], buf);
        uint32_t offset = 0;

        while (offset < m->block_size)
        {
            ext2_dirent_t *d = (ext2_dirent_t *)(buf + offset);

            if (d->rec_len == 0)
                break; // Safety break to prevent infinite loops on corrupt FS

            // Check if this entry has the same name
            if (d->inode && d->name_len == strlen(name) &&
                strncmp(name, d->name, d->name_len) == 0)
            {
                // Entry already exists
                kfree(buf);
                debug_kprintf("Ext2: Entry '%s' already exists in directory\n", name);
                return -1;
            }

            offset += d->rec_len;
        }
    }

    // Tries to find space in existing blocks or allocate a new one.
    for (int i = 0; i < 12; i++)
    {
        // Allocates a new block
        if (dir_inode.i_block[i] == 0)
        {
            uint32_t new_blk = alloc_block(m);
            if (!new_blk)
            {
                kfree(buf);
                return -1;
            }

            dir_inode.i_block[i] = new_blk; // adds the new block
            dir_inode.i_blocks += (m->block_size / 512);
            dir_inode.i_size += m->block_size;

            memset(buf, 0, m->block_size);
            ext2_dirent_t *d = (ext2_dirent_t *)buf;
            d->inode = new_ino;         // adds the new inode in the correct place
            d->rec_len = m->block_size; // First entry takes the whole block
            d->name_len = strlen(name);
            d->file_type = type;
            memcpy(d->name, name, d->name_len);

            write_fs_block(m, new_blk, buf);
            ext2_write_inode(m, dir_ino, &dir_inode);
            kfree(buf);
            return 0;
        }

        // Scans for enough space that already exists (padding)
        read_fs_block(m, dir_inode.i_block[i], buf);
        uint32_t offset = 0;

        while (offset < m->block_size)
        {
            ext2_dirent_t *d = (ext2_dirent_t *)(buf + offset);

            if (d->rec_len == 0)
                break;

            // Calculate actual space required for the current entry
            uint32_t min_len = (8 + d->name_len + 3) & ~3;
            // Calculate space needed for the new entry
            uint32_t needed = (8 + strlen(name) + 3) & ~3;

            // If the current entry's record length is larger than what it needs plus what we need, we can split it.
            if (d->rec_len >= min_len + needed)
            {
                uint32_t old_len = d->rec_len;
                d->rec_len = min_len; // Shrink current entry

                // Create new entry immediately after the shrunk one
                ext2_dirent_t *new_ent = (ext2_dirent_t *)((uint8_t *)d + min_len);
                new_ent->inode = new_ino;
                new_ent->rec_len = old_len - min_len; // New entry takes the rest
                new_ent->name_len = strlen(name);
                new_ent->file_type = type;
                memcpy(new_ent->name, name, new_ent->name_len);

                write_fs_block(m, dir_inode.i_block[i], buf);
                kfree(buf);
                return 0;
            }
            offset += d->rec_len;
        }
    }

    kfree(buf);
    return -1;
}

/**
 * Removes a directory by name
 * Merges the 'rec_len' of the deleted entry into the 'rec_len' of the
 * previous entry, effectively covering the hole.
 *
 * m - Mounted filesystem context.
 * dir_ino - The directory inode number.
 * name - The name of the file/dir to remove.
 * returns an int, 0 on success, -1 on failure.
 */
static int dir_remove_entry(ext2_mount_t *m, uint32_t dir_ino, const char *name)
{
    ext2_inode_t dir_inode;
    if (ext2_read_inode(m, dir_ino, &dir_inode) != 0)
        return -1;

    uint8_t *buf = kmalloc(m->block_size);
    if (!buf)
        return -1;

    for (int i = 0; i < 12; i++)
    {
        if (dir_inode.i_block[i] == 0)
            break;

        read_fs_block(m, dir_inode.i_block[i], buf);
        uint32_t offset = 0;
        ext2_dirent_t *prev = NULL;

        while (offset < m->block_size)
        {
            ext2_dirent_t *d = (ext2_dirent_t *)(buf + offset);

            if (d->rec_len == 0)
                break;

            if (d->inode && d->name_len == strlen(name) &&
                strncmp(name, d->name, d->name_len) == 0)
            {
                if (prev)
                {
                    // Merge this entry's space into the previous entry
                    prev->rec_len += d->rec_len;
                }
                else
                {
                    // If it's the first entry, we just invalidate the inode.
                    d->inode = 0;
                }

                write_fs_block(m, dir_inode.i_block[i], buf);
                kfree(buf);
                return 0;
            }

            prev = d;
            offset += d->rec_len;
        }
    }

    kfree(buf);
    return -1;
}

/**
 * Formats a block device with a fresh Ext2 filesystem.
 *
 * Writes the Superblock, Block Group Descriptor (BGD), Bitmaps,
 * and creates the root directory (inode 2) with entries for '.' and '..'.
 *
 * dev - The block device to format.
 * return an int, 0 on success, -1 on failure.
 */
int ext2_create_filesystem(blockdev_t *dev)
{
    if (!dev)
        return -1;
    debug_kprintf("Ext2: Formatting %s...\n", dev->name);

    const uint32_t BLOCK_SIZE = 1024;
    uint32_t blocks_count = (dev->sector_count * 512) / BLOCK_SIZE;
    uint32_t inodes_count = 1024;

    ext2_mount_t temp_mount;
    temp_mount.dev = dev;
    temp_mount.block_size = BLOCK_SIZE;

    uint8_t *buf = kmalloc(BLOCK_SIZE);
    if (!buf)
        return -1;

    // Setup Superblock
    ext2_superblock_t sb;
    memset(&sb, 0, sizeof(sb));
    sb.s_inodes_count = inodes_count;
    sb.s_blocks_count = blocks_count;
    sb.s_r_blocks_count = 0;
    sb.s_free_blocks_count = blocks_count - 64; // Reserved for metadata overhead
    sb.s_free_inodes_count = inodes_count - 10; // First 10 inodes are reserved
    sb.s_first_data_block = 1;
    sb.s_log_block_size = 0; // 1024 bytes
    sb.s_blocks_per_group = 8192;
    sb.s_frags_per_group = 8192;
    sb.s_inodes_per_group = inodes_count;
    sb.s_magic = EXT2_MAGIC;
    sb.s_rev_level = 1;
    sb.s_inode_size = 128;

    write_superblock(dev, &sb);

    // Setup Block Group Descriptor
    ext2_bgd_t bgd;
    memset(&bgd, 0, sizeof(bgd));
    bgd.bg_block_bitmap = 3;
    bgd.bg_inode_bitmap = 4;
    bgd.bg_inode_table = 5;
    bgd.bg_free_blocks_count = sb.s_free_blocks_count;
    bgd.bg_free_inodes_count = sb.s_free_inodes_count;
    bgd.bg_used_dirs_count = 1; // Root directory

    memset(buf, 0, BLOCK_SIZE);
    memcpy(buf, &bgd, sizeof(bgd));
    write_fs_block(&temp_mount, 2, buf);

    // Write Block Bitmap (Marking metadata blocks as used)
    memset(buf, 0, BLOCK_SIZE);
    for (int i = 0; i < 100; i++)
        buf[i / 8] |= (1 << (i % 8)); // Mark first 100 blocks as used
    write_fs_block(&temp_mount, 3, buf);

    // Write Inode Bitmap (Marking reserved inodes 1-10 as used)
    memset(buf, 0, BLOCK_SIZE);
    for (int i = 0; i < 11; i++)
        buf[i / 8] |= (1 << (i % 8));
    write_fs_block(&temp_mount, 4, buf);

    // Zero out Inode Table
    memset(buf, 0, BLOCK_SIZE);
    for (int i = 0; i < 10; i++)
        write_fs_block(&temp_mount, 5 + i, buf);

    // Create Root Inode
    ext2_inode_t root;
    memset(&root, 0, sizeof(root));
    root.i_mode = 0x41ED; // Directory (0x4000) + chmod 755
    root.i_size = 1024;
    root.i_links_count = 2; // Link to parent and self
    root.i_blocks = 2;      // 512-byte sectors count
    root.i_block[0] = 50;   // Data located at block 50

    // Write root inode to inode table (offset 128 bytes, since it's index 2)
    memset(buf, 0, BLOCK_SIZE);
    memcpy(buf + 128, &root, sizeof(root));
    write_fs_block(&temp_mount, 5, buf);

    // Initialize Root Directory Content (. and ..)
    memset(buf, 0, BLOCK_SIZE);
    ext2_dirent_t *d = (ext2_dirent_t *)buf;
    d->inode = 2;
    d->rec_len = 12;
    d->name_len = 1;
    d->file_type = 2; // Directory
    d->name[0] = '.';

    d = (ext2_dirent_t *)(buf + 12);
    d->inode = 2;
    d->rec_len = BLOCK_SIZE - 12; // Remainder of block
    d->name_len = 2;
    d->file_type = 2;
    d->name[0] = '.';
    d->name[1] = '.';

    write_fs_block(&temp_mount, 50, buf);

    kfree(buf);
    debug_kprintf("Ext2: Format complete.\n");
    return 0;
}

/**
 * Creates a new file in the VFS.
 *
 * dir - Parent directory vnode.
 * name - Name of the file to create.
 * res - Pointer to store the resulting vnode.
 * returns an int, 0 on success.
 */
static int ext2_create(vnode_t *dir, const char *name, vnode_t **res)
{
    if (dir->type != VFS_DIR)
        return -1;
    ext2_mount_t *m = (ext2_mount_t *)dir->mount->fs_data;

    debug_kprintf("Ext2: Creating file '%s' in inode %lu\n", name, dir->inode);

    /* First check if file already exists */
    vnode_t *existing = NULL;
    if (ext2_lookup(dir, name, &existing) == 0)
    {
        kfree(existing);
        debug_kprintf("Ext2: File '%s' already exists\n", name);
        return -1;
    }

    uint32_t ino = alloc_inode(m);
    if (ino == 0)
    {
        debug_kprintf("Ext2: Failed to allocate inode\n");
        return -1;
    }

    ext2_inode_t node;
    memset(&node, 0, sizeof(node));
    node.i_mode = 0x81A4; // Regular File (0x8000) + 644 permissions
    node.i_size = 0;
    node.i_links_count = 1;
    node.i_blocks = 0;
    node.i_atime = node.i_ctime = node.i_mtime = 0;

    ext2_write_inode(m, ino, &node);

    if (dir_add_entry(m, dir->inode, ino, name, 1) != 0)
    {
        free_inode(m, ino);
        debug_kprintf("Ext2: Failed to add directory entry\n");
        return -1;
    }

    vnode_t *v = kmalloc(sizeof(vnode_t));
    memset(v, 0, sizeof(vnode_t));
    v->inode = ino;
    v->type = VFS_FILE;
    v->mount = dir->mount;
    v->ops = &ext2_ops;
    v->size = 0;
    v->refcount = 1;
    *res = v;

    debug_kprintf("Ext2: File created successfully (inode %u)\n", ino);
    return 0;
}

/**
 * Creates a new directory.
 *
 * Handles inode allocation, block allocation for the directory data,
 * "." and ".." initialization, and updating parent link counts.
 *
 * dir - Parent directory vnode.
 * name - Name of new directory.
 * res - Pointer to store resulting vnode.
 * returns an int, 0 on success.
 */
static int ext2_mkdir(vnode_t *dir, const char *name, vnode_t **res)
{
    if (dir->type != VFS_DIR)
        return -1;
    ext2_mount_t *m = (ext2_mount_t *)dir->mount->fs_data;

    debug_kprintf("Ext2: Creating directory '%s' in inode %lu\n", name, dir->inode);

    /* First check if directory already exists */
    vnode_t *existing = NULL;
    if (ext2_lookup(dir, name, &existing) == 0)
    {
        kfree(existing);
        debug_kprintf("Ext2: Directory '%s' already exists\n", name);
        return -1;
    }

    uint32_t ino = alloc_inode(m);
    if (ino == 0)
    {
        debug_kprintf("Ext2: Failed to allocate inode\n");
        return -1;
    }

    uint32_t dir_block = alloc_block(m);
    if (!dir_block)
    {
        free_inode(m, ino);
        debug_kprintf("Ext2: Failed to allocate block\n");
        return -1;
    }

    ext2_inode_t node;
    memset(&node, 0, sizeof(node));
    node.i_mode = 0x41ED; // Directory mode
    node.i_size = m->block_size;
    node.i_links_count = 2; // Links: Parent + Self (.)
    node.i_blocks = m->block_size / 512;
    node.i_block[0] = dir_block;

    // Initialize the new directory block with . and ..
    uint8_t *buf = kmalloc(m->block_size);
    memset(buf, 0, m->block_size);

    ext2_dirent_t *d = (ext2_dirent_t *)buf;
    d->inode = ino;
    d->rec_len = 12;
    d->name_len = 1;
    d->file_type = 2;
    d->name[0] = '.';

    d = (ext2_dirent_t *)(buf + 12);
    d->inode = dir->inode; // Parent inode
    d->rec_len = m->block_size - 12;
    d->name_len = 2;
    d->file_type = 2;
    d->name[0] = '.';
    d->name[1] = '.';

    write_fs_block(m, dir_block, buf);
    kfree(buf);

    ext2_write_inode(m, ino, &node);

    if (dir_add_entry(m, dir->inode, ino, name, 2) != 0)
    {
        free_block(m, dir_block);
        free_inode(m, ino);
        debug_kprintf("Ext2: Failed to add directory entry\n");
        return -1;
    }

    // Update parent inode link count (directories increase parent link count via "..")
    ext2_inode_t parent_inode;
    ext2_read_inode(m, dir->inode, &parent_inode);
    parent_inode.i_links_count++;
    ext2_write_inode(m, dir->inode, &parent_inode);

    m->bgdt[0].bg_used_dirs_count++;
    ext2_sync_metadata(m);

    vnode_t *v = kmalloc(sizeof(vnode_t));
    memset(v, 0, sizeof(vnode_t));
    v->inode = ino;
    v->type = VFS_DIR;
    v->mount = dir->mount;
    v->size = m->block_size;
    v->ops = &ext2_ops;
    v->refcount = 1;
    *res = v;

    debug_kprintf("Ext2: Directory created successfully (inode %u)\n", ino);
    return 0;
}

/**
 * Unlinks (deletes) a file.
 * Releases all blocks associated with the file and marks the inode as free.
 *
 * dir - Parent directory vnode.
 * name - File name to unlink.
 * returns an int, 0 on success.
 */
static int ext2_unlink(vnode_t *dir, const char *name)
{
    if (dir->type != VFS_DIR)
        return -1;
    ext2_mount_t *m = (ext2_mount_t *)dir->mount->fs_data;

    debug_kprintf("Ext2: Unlinking '%s' from inode %lu\n", name, dir->inode);

    vnode_t *target = NULL;
    if (ext2_lookup(dir, name, &target) != 0)
    {
        debug_kprintf("Ext2: File not found\n");
        return -1;
    }

    if (target->type == VFS_DIR)
    {
        kfree(target);
        debug_kprintf("Ext2: Cannot unlink directory (use rmdir)\n");
        return -1;
    }

    ext2_inode_t inode;
    ext2_read_inode(m, target->inode, &inode);

    // Free all direct blocks used by the file
    for (int i = 0; i < 12 && inode.i_block[i]; i++)
    {
        free_block(m, inode.i_block[i]);
    }

    if (dir_remove_entry(m, dir->inode, name) != 0)
    {
        kfree(target);
        debug_kprintf("Ext2: Failed to remove directory entry\n");
        return -1;
    }

    free_inode(m, target->inode);
    kfree(target);

    debug_kprintf("Ext2: File unlinked successfully\n");
    return 0;
}

/**
 * Truncates a file to zero size, freeing all its blocks.
 */
static int ext2_truncate(vnode_t *node)
{
    if (!node || node->type != VFS_FILE)
        return -1;

    ext2_mount_t *m = (ext2_mount_t *)node->mount->fs_data;
    ext2_inode_t inode;

    if (ext2_read_inode(m, node->inode, &inode) != 0)
        return -1;

    for (int i = 0; i < 12; i++)
    {
        if (inode.i_block[i] != 0)
        {
            free_block(m, inode.i_block[i]);
            inode.i_block[i] = 0;
        }
    }

    if (inode.i_block[12] != 0)
    {
        uint32_t *ind_buf = kmalloc(m->block_size);
        if (ind_buf)
        {
            read_fs_block(m, inode.i_block[12], (uint8_t *)ind_buf);
            uint32_t ptrs = m->block_size / sizeof(uint32_t);
            for (uint32_t i = 0; i < ptrs; i++)
            {
                if (ind_buf[i] != 0)
                {
                    free_block(m, ind_buf[i]);
                }
            }
            kfree(ind_buf);
        }
        free_block(m, inode.i_block[12]);
        inode.i_block[12] = 0;
    }

    inode.i_size = 0;
    inode.i_blocks = 0;

    if (ext2_write_inode(m, node->inode, &inode) != 0)
        return -1;

    node->size = 0;
    return 0;
}

/**
 * Removes an empty directory.
 * Checks if the directory is empty (only contains . and ..) before removal.
 *
 * dir - Parent directory vnode.
 * name - Directory name to remove.
 * returns an int, 0 on success.
 */
static int ext2_rmdir(vnode_t *dir, const char *name)
{
    if (dir->type != VFS_DIR)
        return -1;
    ext2_mount_t *m = (ext2_mount_t *)dir->mount->fs_data;

    debug_kprintf("Ext2: Removing directory '%s' from inode %lu\n", name, dir->inode);

    vnode_t *target = NULL;
    if (ext2_lookup(dir, name, &target) != 0)
    {
        debug_kprintf("Ext2: Directory not found\n");
        return -1;
    }

    if (target->type != VFS_DIR)
    {
        kfree(target);
        debug_kprintf("Ext2: Not a directory\n");
        return -1;
    }

    ext2_inode_t inode;
    ext2_read_inode(m, target->inode, &inode);

    // Check if directory is empty
    uint8_t *buf = kmalloc(m->block_size);
    if (!buf)
    {
        kfree(target);
        return -1;
    }
    if (inode.i_block[0])
    {
        read_fs_block(m, inode.i_block[0], buf);
        uint32_t offset = 0;
        int entry_count = 0;

        while (offset < m->block_size)
        {
            ext2_dirent_t *d = (ext2_dirent_t *)(buf + offset);
            if (d->rec_len == 0)
                break;
            if (d->inode)
                entry_count++;
            offset += d->rec_len;
        }

        // More than 2 entries ('.' and '..') implies it is not empty
        if (entry_count > 2)
        {
            kfree(buf);
            kfree(target);
            debug_kprintf("Ext2: Directory not empty\n");
            return -1;
        }
    }
    kfree(buf);

    // Free directory blocks
    for (int i = 0; i < 12 && inode.i_block[i]; i++)
    {
        free_block(m, inode.i_block[i]);
    }

    if (dir_remove_entry(m, dir->inode, name) != 0)
    {
        kfree(target);
        debug_kprintf("Ext2: Failed to remove directory entry\n");
        return -1;
    }

    // Update parent link count
    ext2_inode_t parent_inode;
    ext2_read_inode(m, dir->inode, &parent_inode);
    parent_inode.i_links_count--;
    ext2_write_inode(m, dir->inode, &parent_inode);

    m->bgdt[0].bg_used_dirs_count--;
    ext2_sync_metadata(m);

    free_inode(m, target->inode);
    kfree(target);

    debug_kprintf("Ext2: Directory removed successfully\n");
    return 0;
}

/**
 * Finds a file inside a directory.
 *
 * dir - The directory to search in.
 * name - The filename to look for.
 * res - Pointer to store resulting vnode.
 * returns and int, 0 found, -1 not found.
 */
static int ext2_lookup(vnode_t *dir, const char *name, vnode_t **res)
{
    ext2_mount_t *m = (ext2_mount_t *)dir->mount->fs_data;
    ext2_inode_t inode;
    ext2_read_inode(m, dir->inode, &inode);

    uint8_t *buf = kmalloc(m->block_size);
    if (!buf)
        return -1;

    for (int i = 0; i < 12; i++)
    {
        if (inode.i_block[i] == 0)
            break;

        read_fs_block(m, inode.i_block[i], buf);
        uint32_t off = 0;
        while (off < m->block_size)
        {
            ext2_dirent_t *d = (ext2_dirent_t *)(buf + off);
            if (d->rec_len == 0)
                break;

            if (d->inode && d->name_len == strlen(name) &&
                strncmp(name, d->name, d->name_len) == 0)
            {
                vnode_t *v = kmalloc(sizeof(vnode_t));
                if (!v)
                {
                    kfree(buf);
                    return -1;
                }

                ext2_inode_t target;
                ext2_read_inode(m, d->inode, &target);

                memset(v, 0, sizeof(vnode_t));
                v->inode = d->inode;
                v->mount = dir->mount;
                v->ops = &ext2_ops;
                // Check if directory flag (0x4000) is set in mode
                v->type = (target.i_mode & 0x4000) ? VFS_DIR : VFS_FILE;
                v->size = target.i_size;
                v->refcount = 1;
                kfree(buf);
                *res = v;
                return 0;
            }
            off += d->rec_len;
        }
    }
    kfree(buf);
    return -1;
}

/**
 * Reads data from an inode.
 * returns an int, Number of bytes read.
 */
static int ext2_read(vnode_t *node, void *buf, size_t size, uint64_t offset)
{
    ext2_mount_t *m = (ext2_mount_t *)node->mount->fs_data;
    ext2_inode_t inode;
    ext2_read_inode(m, node->inode, &inode);

    if (offset >= inode.i_size)
        return 0;
    if (offset + size > inode.i_size)
        size = inode.i_size - offset;

    uint32_t start_blk = offset / m->block_size;
    uint32_t end_blk = (offset + size - 1) / m->block_size;
    uint32_t copied = 0;

    // Number of block pointers that fit in one indirect block
    uint32_t ptrs_per_block = m->block_size / sizeof(uint32_t);

    uint8_t *tmp = kmalloc(m->block_size);
    uint8_t *ind_buf = NULL; // holds singly-indirect table

    for (uint32_t b = start_blk; b <= end_blk; b++)
    {
        uint32_t phys_block = 0;

        if (b < 12)
        {
            // direct block
            phys_block = inode.i_block[b];
        }
        else if (b < 12 + ptrs_per_block)
        {
            // Singly-indirect block (i_block[12])
            if (inode.i_block[12] == 0)
            {
                phys_block = 0; // treat as a hole
            }
            else
            {
                if (!ind_buf)
                {
                    ind_buf = kmalloc(m->block_size);
                    read_fs_block(m, inode.i_block[12], ind_buf);
                }
                uint32_t idx = b - 12;
                phys_block = ((uint32_t *)ind_buf)[idx];
            }
        }
        else
        {
            break; // Doubly/triply-indirect not implemented
        }

        if (phys_block == 0)
        {
            memset(tmp, 0, m->block_size); // Sparse hole reads as zero
        }
        else
        {
            read_fs_block(m, phys_block, tmp);
        }

        uint32_t block_off = (b == start_blk) ? (offset % m->block_size) : 0;
        uint32_t chunk = m->block_size - block_off;
        if (chunk > (size - copied))
            chunk = size - copied;

        memcpy((uint8_t *)buf + copied, tmp + block_off, chunk);
        copied += chunk;
    }

    if (ind_buf)
        kfree(ind_buf);
    kfree(tmp);
    return copied;
}

/**
 * Writes data to an inode.
 * Allocates blocks on demand if they do not exist (sparse write support).
 * returns an int, Number of bytes written.
 */
static int ext2_write(vnode_t *node, const void *buf, size_t size, uint64_t offset)
{
    ext2_mount_t *m = (ext2_mount_t *)node->mount->fs_data;
    ext2_inode_t inode;
    ext2_read_inode(m, node->inode, &inode);

    debug_kprintf("Ext2: Writing %u bytes to inode %lu at offset %lu\n",
                  (uint32_t)size, node->inode, offset);

    uint32_t start_blk = offset / m->block_size;
    uint32_t end_blk = (offset + size - 1) / m->block_size;
    uint32_t copied = 0;

    // Number of block pointers that fit in one indirect block
    uint32_t ptrs_per_block = m->block_size / sizeof(uint32_t);

    uint8_t *tmp = kmalloc(m->block_size);
    uint8_t *ind_buf = NULL; // singly-indirect pointer table
    int ind_dirty = 0;

    for (uint32_t b = start_blk; b <= end_blk; b++)
    {
        uint32_t phys_block = 0;
        int is_indirect = 0;
        uint32_t ind_idx = 0;

        if (b < 12)
        {
            // Direct block
            phys_block = inode.i_block[b];

            if (phys_block == 0)
            {
                phys_block = alloc_block(m);
                if (!phys_block)
                {
                    debug_kprintf("Ext2: Failed to allocate direct block\n");
                    break;
                }
                debug_kprintf("Ext2: Allocated direct block %u for inode %lu\n", phys_block, node->inode);
                inode.i_block[b] = phys_block;
                inode.i_blocks += (m->block_size / 512);
                memset(tmp, 0, m->block_size);
            }
            else
            {
                read_fs_block(m, phys_block, tmp);
            }
        }
        else if (b < 12 + ptrs_per_block)
        {
            // Singly-indirect block
            is_indirect = 1;
            ind_idx = b - 12;

            // Allocate the indirect pointer block itself if needed
            if (inode.i_block[12] == 0)
            {
                uint32_t ind_blk = alloc_block(m);
                if (!ind_blk)
                {
                    debug_kprintf("Ext2: Failed to allocate indirect block\n");
                    break;
                }
                debug_kprintf("Ext2: Allocated indirect block %u for inode %lu\n",
                              ind_blk, node->inode);
                inode.i_block[12] = ind_blk;
                inode.i_blocks += (m->block_size / 512);

                ind_buf = kmalloc(m->block_size);
                memset(ind_buf, 0, m->block_size);
            }
            else if (!ind_buf)
            {
                ind_buf = kmalloc(m->block_size);
                read_fs_block(m, inode.i_block[12], ind_buf);
            }

            phys_block = ((uint32_t *)ind_buf)[ind_idx];

            if (phys_block == 0)
            {
                phys_block = alloc_block(m);
                if (!phys_block)
                {
                    debug_kprintf("Ext2: Failed to allocate indirect data block\n");
                    break;
                }
                debug_kprintf("Ext2: Allocated indirect data block %u for inode %lu\n",
                              phys_block, node->inode);
                ((uint32_t *)ind_buf)[ind_idx] = phys_block;
                inode.i_blocks += (m->block_size / 512);
                ind_dirty = 1;
                memset(tmp, 0, m->block_size);
            }
            else
            {
                read_fs_block(m, phys_block, tmp);
            }
        }
        else
        {
            break; // Doubly/triply-indirect not implemented
        }

        uint32_t block_off = (b == start_blk) ? (offset % m->block_size) : 0;
        uint32_t chunk = m->block_size - block_off;
        if (chunk > (size - copied))
            chunk = size - copied;

        memcpy(tmp + block_off, (uint8_t *)buf + copied, chunk);
        write_fs_block(m, phys_block, tmp);

        if (is_indirect)
            ind_dirty = 1;

        copied += chunk;
    }

    // Flush indirect pointer table if it was modified
    if (ind_buf && ind_dirty)
        write_fs_block(m, inode.i_block[12], ind_buf);

    if (ind_buf)
        kfree(ind_buf);
    kfree(tmp);

    if (offset + copied > inode.i_size)
    {
        inode.i_size = offset + copied;
        node->size = inode.i_size;
    }

    ext2_write_inode(m, node->inode, &inode);
    debug_kprintf("Ext2: Write complete. %u bytes written, new size: %u\n", copied, inode.i_size);

    return copied;
}
/**
 * Mounts an Ext2 filesystem.
 * Reads superblock, block group descriptors, and initializes the root vnode.
 */
static int ext2_mount(const char *devname, mount_t **mnt)
{
    blockdev_t *dev = blockdev_find(devname);
    if (!dev)
        return -1;

    ext2_superblock_t sb;
    if (read_superblock(dev, &sb) != 0)
        return -1;

    if (sb.s_magic != EXT2_MAGIC)
        return -1;

    ext2_mount_t *m = kmalloc(sizeof(ext2_mount_t));
    m->dev = dev;
    memcpy(&m->sb, &sb, sizeof(ext2_superblock_t));
    m->block_size = 1024 << sb.s_log_block_size;
    m->inode_size = (sb.s_rev_level >= 1) ? sb.s_inode_size : 128;

    m->bgdt = kmalloc(m->block_size);
    read_fs_block(m, 2, m->bgdt); // Assumes 1k blocks, BGD usually at block 2

    *mnt = kmalloc(sizeof(mount_t));
    (*mnt)->fs_data = m;
    (*mnt)->fs = &ext2_fs;

    vnode_t *root = kmalloc(sizeof(vnode_t));
    memset(root, 0, sizeof(vnode_t));
    root->inode = 2; // Ext2 Root Inode is always 2
    root->type = VFS_DIR;
    root->mount = *mnt;
    root->ops = &ext2_ops;
    root->refcount = 1;
    (*mnt)->root = root;

    return 0;
}

/**
 * Lists entries in a directory.
 */
static int ext2_readdir(vnode_t *node, dirent_t *dirent, uint64_t index)
{
    if (node->type != VFS_DIR)
        return -1;

    ext2_mount_t *m = (ext2_mount_t *)node->mount->fs_data;
    ext2_inode_t inode;
    ext2_read_inode(m, node->inode, &inode);

    uint8_t *buf = kmalloc(m->block_size);
    if (!buf)
        return -1;

    uint64_t current_index = 0;

    for (int i = 0; i < 12; i++)
    {
        if (inode.i_block[i] == 0)
            break;

        read_fs_block(m, inode.i_block[i], buf);
        uint32_t offset = 0;

        while (offset < m->block_size)
        {
            ext2_dirent_t *d = (ext2_dirent_t *)(buf + offset);

            if (d->rec_len == 0)
                break;

            if (d->inode)
            {
                if (current_index == index)
                {
                    dirent->inode = d->inode;
                    dirent->type = d->file_type;

                    uint32_t copy_len = d->name_len;
                    if (copy_len >= VFS_MAX_NAME)
                        copy_len = VFS_MAX_NAME - 1;

                    memcpy(dirent->name, d->name, copy_len);
                    dirent->name[copy_len] = '\0';

                    kfree(buf);
                    return 0;
                }
                current_index++;
            }

            offset += d->rec_len;
        }
    }

    kfree(buf);
    return -1;
}

static void ext2_release(vnode_t *node)
{
    kfree(node);
}