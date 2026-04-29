/*
 * Licensed under MIT License - URIX project.
 * kmalloc.c - kmalloc and kfree implementation
 */

#include <memory/kmalloc.h>
#include <memory/physical/pmm.h>
#include <memory/vmm.h>
#include <memory/memory_utils.h>
#include <lib/panic.h>
#include <string.h>
#include <lib/print.h>
#include <stdint.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif /* PAGE_SIZE */
#define KMALLOC_MAX_SLAB 2048
#define KMALLOC_MIN_SLAB 16
#define KMALLOC_CLASS_COUNT 8
#define KMALLOC_CANARY 0xDEADBEEFCAFEBABEULL

static const size_t slab_sizes[KMALLOC_CLASS_COUNT] = {16, 32, 64, 128, 256, 512, 1024, 2048};

typedef struct kmalloc_header
{
    uint64_t canary;
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

static int size_to_class(size_t size)
{
    for (int i = 0; i < KMALLOC_CLASS_COUNT; i++)
        if (size <= slab_sizes[i])
            return i;
    return -1;
}

static void *alloc_pages(size_t pages)
{
    uint64_t first_virt = 0;

    for (size_t i = 0; i < pages; i++)
    {
        uint64_t phys = pmm_alloc_frame();
        if (!phys)
            PANIC("kmalloc: out of physical memory");
            
        uint64_t virt = (uint64_t)phys_to_virt(phys);
        
        if (i == 0)
            first_virt = virt;
    }

    kmalloc_header_t *hdr = (kmalloc_header_t *)first_virt;
    hdr->canary = KMALLOC_CANARY;
    hdr->flags = KMALLOC_FLAG_PAGE;
    hdr->class = 0;
    hdr->pages = pages;

    return (void *)(first_virt + sizeof(kmalloc_header_t));
}

static void free_pages(void *ptr)
{
    kmalloc_header_t *hdr = (kmalloc_header_t *)((uint8_t *)ptr - sizeof(kmalloc_header_t));
    uint64_t base = (uint64_t)hdr;
    uint32_t pages = hdr->pages;

    for (uint32_t i = 0; i < pages; i++) {
        uint64_t virt = base + i * PAGE_SIZE;
        pmm_free_frame(virt_to_phys((void *)virt));
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
    size_t usable = PAGE_SIZE - sizeof(kmalloc_header_t);
    size_t count = usable / block;

    kmalloc_header_t *hdr = (kmalloc_header_t *)virt;
    hdr->canary = KMALLOC_CANARY;
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

    /* For small allocations, use slabs directly without header overhead */
    int class = size_to_class(size);
    if (class >= 0)
    {
        slab_class_t *sc = &slab_classes[class];

        if (!sc->free_list)
            slab_refill(class);

        free_block_t *blk = sc->free_list;
        sc->free_list = blk->next;
        sc->used_blocks++;

        return (void *)blk;
    }

    /* Large allocation - use pages */
    size_t total = size + sizeof(kmalloc_header_t);
    size_t pages = align_up(total, PAGE_SIZE) / PAGE_SIZE;
    return alloc_pages(pages);
}

void kfree(void *ptr)
{
    if (!ptr)
        return;

    kmalloc_header_t *hdr_before = (kmalloc_header_t *)((uint8_t *)ptr - sizeof(kmalloc_header_t));

    if (hdr_before->flags & KMALLOC_FLAG_PAGE)
    {
        if (hdr_before->canary != KMALLOC_CANARY)
        {
            kprintf("[kfree CORRUPTION] page-alloc header at 0x%llx has bad canary 0x%llx (ptr=0x%llx)\n", (uint64_t)hdr_before, hdr_before->canary, (uint64_t)ptr);
            PANIC("heap corruption detected in kfree");
        }
        free_pages(ptr);
        return;
    }

    uint64_t page_base = ((uint64_t)ptr) & ~((uint64_t)PAGE_SIZE - 1);
    kmalloc_header_t *hdr = (kmalloc_header_t *)page_base;

    if (hdr->canary != KMALLOC_CANARY)
    {
        kprintf("[kfree CORRUPTION] slab header at 0x%llx has bad canary 0x%llx (ptr=0x%llx)\n", (uint64_t)hdr, hdr->canary, (uint64_t)ptr);
        PANIC("heap corruption detected in kfree");
    }

    if (hdr->flags & KMALLOC_FLAG_SLAB)
    {
        if (hdr->class >= KMALLOC_CLASS_COUNT)
            PANIC("kfree: invalid slab class");

        slab_class_t *sc = &slab_classes[hdr->class];
        free_block_t *blk = (free_block_t *)ptr;
        blk->next = sc->free_list;
        sc->free_list = blk;
        sc->used_blocks--;
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
        kprintf("  %06luB: used %lu / total %lu\n",
                sc->block_size,
                sc->used_blocks,
                sc->total_blocks);
    }
}