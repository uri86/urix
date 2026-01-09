/*
 * Licensed under MIT License - URIX project.
 * kmalloc.c - kmalloc and kfree implementation
 */

#include <memory/kmalloc.h>
#include <memory/physical/pmm.h>
#include <memory/vmm.h>
#include <lib/panic.h>
#include <lib/string.h>
#include <lib/print.h>
#include <stdint.h>

#ifndef PAGE_SIZE
    #define PAGE_SIZE 4096
#endif /* PAGE_SIZE */
#define KMALLOC_MAX_SLAB 2048
#define KMALLOC_MIN_SLAB 16
#define KMALLOC_CLASS_COUNT 8

static const size_t slab_sizes[KMALLOC_CLASS_COUNT] = {16, 32, 64, 128, 256, 512, 1024, 2048};

typedef struct kmalloc_header
{
    uint16_t flags;
    uint16_t class;
    uint32_t pages;
} kmalloc_header_t;

#define KMALLOC_FLAG_SLAB 0x1
#define KMALLOC_FLAG_PAGE 0x2

typedef struct free_block
{
    struct free_block *next;
} free_block_t;

typedef struct slab_class
{
    size_t block_size;
    free_block_t *free_list;
    uint64_t total_blocks;
    uint64_t used_blocks;
} slab_class_t;

static slab_class_t slab_classes[KMALLOC_CLASS_COUNT];
static inline size_t align_up(size_t x, size_t a) { return (x + a - 1) & ~(a - 1); }
static int size_to_class(size_t size)
{
    for (int i = 0; i < KMALLOC_CLASS_COUNT; i++)
        if (size <= slab_sizes[i])
            return i;
    return -1;
}

static void *alloc_pages(size_t pages)
{
    // uint64_t first_phys = 0;
    uint64_t first_virt = 0;

    for (size_t i = 0; i < pages; i++)
    {
        uint64_t phys = pmm_alloc_frame();
        if (!phys)
            PANIC("kmalloc: out of physical memory");

        uint64_t virt = (uint64_t)phys_to_virt(phys);

        vmm_map_page(NULL, virt, phys, VMM_KERNEL_FLAGS);

        if (i == 0)
        {
            // first_phys = phys;
            first_virt = virt;
        }
    }

    kmalloc_header_t *hdr = (kmalloc_header_t *)first_virt;
    hdr->flags = KMALLOC_FLAG_PAGE;
    hdr->pages = pages;

    return (void *)(first_virt + sizeof(kmalloc_header_t));
}

static void free_pages(void *ptr)
{
    kmalloc_header_t *hdr = (kmalloc_header_t *)((uint8_t *)ptr - sizeof(kmalloc_header_t));

    uint64_t base = (uint64_t)hdr;

    for (uint32_t i = 0; i < hdr->pages; i++)
    {
        uint64_t virt = base + i * PAGE_SIZE;
        uint64_t phys = vmm_unmap_page(NULL, virt);
        if (phys)
            pmm_free_frame(phys);
    }
}

static void slab_refill(int class)
{
    slab_class_t *sc = &slab_classes[class];
    size_t block = sc->block_size;

    uint64_t phys = pmm_alloc_frame();
    if (!phys)
        PANIC("kmalloc: slab refill OOM");

    uint64_t virt = (uint64_t)phys_to_virt(phys);
    vmm_map_page(NULL, virt, phys, VMM_KERNEL_FLAGS);

    size_t usable = PAGE_SIZE - sizeof(kmalloc_header_t);
    size_t count = usable / block;

    kmalloc_header_t *hdr = (kmalloc_header_t *)virt;
    hdr->flags = KMALLOC_FLAG_SLAB;
    hdr->class = class;
    hdr->pages = 1;

    uint8_t *data = (uint8_t *)virt + sizeof(kmalloc_header_t);

    for (size_t i = 0; i < count; i++)
    {
        free_block_t *blk = (free_block_t *)(data + i * block);
        blk->next = sc->free_list;
        sc->free_list = blk;
    }

    sc->total_blocks += count;
}

void kmalloc_init(void)
{
    for (int i = 0; i < KMALLOC_CLASS_COUNT; i++)
    {
        slab_classes[i].block_size = slab_sizes[i];
        slab_classes[i].free_list = NULL;
        slab_classes[i].total_blocks = 0;
        slab_classes[i].used_blocks = 0;
    }
}

void *kmalloc(size_t size)
{
    if (size == 0)
        return NULL;

    size_t total = size + sizeof(kmalloc_header_t);

    int class = size_to_class(total);
    if (class < 0)
    {
        size_t pages = align_up(total, PAGE_SIZE) / PAGE_SIZE;
        return alloc_pages(pages);
    }

    slab_class_t *sc = &slab_classes[class];

    if (!sc->free_list)
        slab_refill(class);

    free_block_t *blk = sc->free_list;
    sc->free_list = blk->next;
    sc->used_blocks++;

    return (void *)blk;
}

void kfree(void *ptr)
{
    if (!ptr)
        return;

    /* Find the page base address to locate the header */
    uint64_t page_base = ((uint64_t)ptr) & ~(PAGE_SIZE - 1);
    kmalloc_header_t *hdr = (kmalloc_header_t *)page_base;

    /* Check if this looks like a valid header */
    if (hdr->flags == 0 || (hdr->flags & ~(KMALLOC_FLAG_SLAB | KMALLOC_FLAG_PAGE)))
    {
        PANIC("kfree: invalid pointer or corrupted header");
    }

    if (hdr->flags & KMALLOC_FLAG_SLAB)
    {
        if (hdr->class >= KMALLOC_CLASS_COUNT)
        {
            PANIC("kfree: invalid slab class");
        }

        slab_class_t *sc = &slab_classes[hdr->class];
        free_block_t *blk = (free_block_t *)ptr;

        blk->next = sc->free_list;
        sc->free_list = blk;
        sc->used_blocks--;
        return;
    }

    if (hdr->flags & KMALLOC_FLAG_PAGE)
    {
        free_pages(ptr);
        return;
    }

    PANIC("kfree: unknown allocation type");
}

void kmalloc_print_stats(void)
{
    kprintf("kmalloc slab usage:\n");
    for (int i = 0; i < KMALLOC_CLASS_COUNT; i++)
    {
        slab_class_t *sc = &slab_classes[i];
        kprintf("  %4luB: used %lu / total %lu\n",
                sc->block_size,
                sc->used_blocks,
                sc->total_blocks);
    }
}