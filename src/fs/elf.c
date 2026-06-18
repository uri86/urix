/*
 * Licensed under MIT License - URIX project.
 * elf.c - ELF64 executable loader
 */
#include <fs/elf.h>
#include <memory/vmm.h>
#include <memory/physical/pmm.h>
#include <memory/kmalloc.h>
#include <fs/vfs.h>
#include <string.h>
#include <lib/print.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#define PAGE_ALIGN_DOWN(x) ((x) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(x) (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

int elf_validate(const elf64_ehdr_t *hdr)
{
    // check for null pointer
    if (!hdr)
        return -1;

    // check ELF magic number
    if (hdr->e_ident[0] != 0x7F ||
        hdr->e_ident[1] != 'E' ||
        hdr->e_ident[2] != 'L' ||
        hdr->e_ident[3] != 'F')
    {
        debug_kprintf("ELF: Invalid magic number\n");
        return -1;
    }

    // make sure it's a 64-bit little-endian executable for x86-64
    if (hdr->e_ident[4] != ELFCLASS64)
    {
        debug_kprintf("ELF: Not a 64-bit ELF\n");
        return -1;
    }

    // check the data encoding
    if (hdr->e_ident[5] != ELFDATA2LSB)
    {
        debug_kprintf("ELF: Not little endian\n");
        return -1;
    }

    // check the type
    if (hdr->e_type != ET_EXEC && hdr->e_type != ET_DYN)
    {
        debug_kprintf("ELF: Not an executable (type=%u)\n", hdr->e_type);
        return -1;
    }

    // make sure it's for the x86-64 architecture
    if (hdr->e_machine != EM_X86_64)
    {
        debug_kprintf("ELF: Not x86-64 architecture\n");
        return -1;
    }

    debug_kprintf("ELF: Valid 64-bit x86-64 executable\n");
    return 0;
}

static int elf_copy_to_mapped(address_space_t *addr_space, uint64_t dst_vaddr, const uint8_t *src, uint64_t size)
{
    uint64_t copied = 0;

    // Loop through the size in chunks that fit within a page
    while (copied < size)
    {
        uint64_t page_vaddr = PAGE_ALIGN_DOWN(dst_vaddr + copied);
        uint64_t page_offset = (dst_vaddr + copied) - page_vaddr;

        // Calculate how much to copy in this iteration (up to the end of the page)
        uint64_t chunk = PAGE_SIZE - page_offset;
        if (chunk > size - copied)
            chunk = size - copied;

        uint64_t phys = vmm_get_physical(addr_space, page_vaddr);
        if (phys == 0)
        {
            debug_kprintf("ELF: vmm_get_physical returned 0 for vaddr 0x%lx\n",
                          page_vaddr);
            return -1;
        }

        // Copy the data from the source to the mapped physical memory
        uint8_t *kptr = (uint8_t *)phys_to_virt(phys) + page_offset;
        memcpy(kptr, src + copied, chunk);

        copied += chunk;
    }

    return 0;
}

static int elf_zero_mapped(address_space_t *addr_space, uint64_t dst_vaddr, uint64_t size)
{
    uint64_t zeroed = 0;

    while (zeroed < size)
    {
        uint64_t page_vaddr = PAGE_ALIGN_DOWN(dst_vaddr + zeroed);
        uint64_t page_offset = (dst_vaddr + zeroed) - page_vaddr;

        uint64_t chunk = PAGE_SIZE - page_offset;
        if (chunk > size - zeroed)
            chunk = size - zeroed;

        uint64_t phys = vmm_get_physical(addr_space, page_vaddr);
        if (phys == 0)
        {
            debug_kprintf("ELF: vmm_get_physical returned 0 for BSS vaddr 0x%lx\n",
                          page_vaddr);
            return -1;
        }

        uint8_t *kptr = (uint8_t *)phys_to_virt(phys) + page_offset;
        memset(kptr, 0, chunk);

        zeroed += chunk;
    }

    return 0;
}

int elf_load(const void *data, size_t size, address_space_t *addr_space, elf_info_t *info)
{
    if (!data || !addr_space || !info)
        return -1;

    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)data;

    if (elf_validate(ehdr) != 0)
        return -1;

    debug_kprintf("ELF: Loading executable...\n");
    debug_kprintf("ELF: Entry point: 0x%lx\n", ehdr->e_entry);
    debug_kprintf("ELF: Program headers: %u at offset 0x%lx\n", ehdr->e_phnum, ehdr->e_phoff);

    const elf64_phdr_t *phdr =
        (const elf64_phdr_t *)((const uint8_t *)data + ehdr->e_phoff);

    uint64_t lowest_addr = 0xFFFFFFFFFFFFFFFFULL;
    uint64_t highest_addr = 0;

    /* determine address range */
    for (int i = 0; i < ehdr->e_phnum; i++)
    {
        if (phdr[i].p_type != PT_LOAD)
            continue;

        if (phdr[i].p_vaddr < lowest_addr)
            lowest_addr = phdr[i].p_vaddr;

        uint64_t end = phdr[i].p_vaddr + phdr[i].p_memsz;
        if (end > highest_addr)
            highest_addr = end;
    }

    debug_kprintf("ELF: Address range: 0x%lx - 0x%lx\n", lowest_addr, highest_addr);

    /* load segments */
    for (int i = 0; i < ehdr->e_phnum; i++)
    {
        if (phdr[i].p_type != PT_LOAD)
            continue;

        debug_kprintf("ELF: Loading segment %d:\n", i);
        debug_kprintf("  Virtual addr: 0x%lx\n", phdr[i].p_vaddr);
        debug_kprintf("  File size:    0x%lx\n", phdr[i].p_filesz);
        debug_kprintf("  Memory size:  0x%lx\n", phdr[i].p_memsz);
        debug_kprintf("  Flags: %c%c%c\n", (phdr[i].p_flags & PF_R) ? 'R' : '-', (phdr[i].p_flags & PF_W) ? 'W' : '-', (phdr[i].p_flags & PF_X) ? 'X' : '-');

        uint64_t vaddr_aligned = PAGE_ALIGN_DOWN(phdr[i].p_vaddr);
        uint64_t vaddr_end = PAGE_ALIGN_UP(phdr[i].p_vaddr + phdr[i].p_memsz);
        uint64_t num_pages = (vaddr_end - vaddr_aligned) / PAGE_SIZE;

        debug_kprintf("  Mapping pages: 0x%lx - 0x%lx (%lu pages)\n",
                      vaddr_aligned, vaddr_end, num_pages);

        uint64_t flags = VMM_USER | VMM_PRESENT;
        if (phdr[i].p_flags & PF_W)
            flags |= VMM_WRITE;

        /* allocate and map physical frames into the target address space */
        for (uint64_t page = 0; page < num_pages; page++)
        {
            uint64_t vaddr = vaddr_aligned + (page * PAGE_SIZE);
            uint64_t phys = pmm_alloc_frame();

            if (phys == 0)
            {
                debug_kprintf("ELF: Failed to allocate physical page\n");
                return -1;
            }

            if (vmm_map_page(addr_space, vaddr, phys, flags) != 0)
            {
                debug_kprintf("ELF: Failed to map page at 0x%lx\n", vaddr);
                pmm_free_frame(phys);
                return -1;
            }
        }

        /* copy segment data into the mapped frames. */
        if (phdr[i].p_filesz > 0)
        {
            const uint8_t *src = (const uint8_t *)data + phdr[i].p_offset;

            debug_kprintf("  Copying 0x%lx bytes from file offset 0x%lx to vaddr 0x%lx\n",
                          phdr[i].p_filesz, phdr[i].p_offset, phdr[i].p_vaddr);

            if (elf_copy_to_mapped(addr_space, phdr[i].p_vaddr,
                                   src, phdr[i].p_filesz) != 0)
            {
                debug_kprintf("ELF: Failed to copy segment data\n");
                return -1;
            }
        }

        // zero out the remaining memory (BSS section)
        if (phdr[i].p_memsz > phdr[i].p_filesz)
        {
            uint64_t bss_start = phdr[i].p_vaddr + phdr[i].p_filesz;
            uint64_t bss_size = phdr[i].p_memsz - phdr[i].p_filesz;

            debug_kprintf("  Zeroing BSS: 0x%lx - 0x%lx (%lu bytes)\n",
                          bss_start, bss_start + bss_size, bss_size);

            if (elf_zero_mapped(addr_space, bss_start, bss_size) != 0)
            {
                debug_kprintf("ELF: Failed to zero BSS\n");
                return -1;
            }
        }
    }

    info->entry_point = ehdr->e_entry;
    info->base_address = lowest_addr;
    info->brk = PAGE_ALIGN_UP(highest_addr);
    info->stack_top = USER_STACK_TOP;

    debug_kprintf("ELF: Loaded successfully!\n");
    debug_kprintf("  Entry point:  0x%lx\n", info->entry_point);
    debug_kprintf("  Base address: 0x%lx\n", info->base_address);
    debug_kprintf("  Break (heap): 0x%lx\n", info->brk);
    debug_kprintf("  Stack top:    0x%lx\n", info->stack_top);

    return 0;
}

int elf_load_from_file(const char *path, address_space_t *addr_space, elf_info_t *info)
{
    if (!path || !addr_space || !info)
        return -1;

    debug_kprintf("ELF: Loading executable from '%s'\n", path);

    file_t *file;
    // open the file using the VFS
    if (vfs_open(path, VFS_READ, &file) != 0)
    {
        debug_kprintf("ELF: Failed to open file '%s'\n", path);
        return -1;
    }

    uint64_t file_size = file->vnode->size;
    debug_kprintf("ELF: File size: %lu bytes\n", file_size);

    if (file_size < sizeof(elf64_ehdr_t))
    {
        debug_kprintf("ELF: File too small to be an ELF\n");
        vfs_close(file);
        return -1;
    }

    void *buffer = kmalloc(file_size);
    if (!buffer)
    {
        debug_kprintf("ELF: Failed to allocate read buffer\n");
        vfs_close(file);
        return -1;
    }

    int bytes_read = vfs_read(file, buffer, file_size);
    vfs_close(file);

    if (bytes_read != (int)file_size)
    {
        debug_kprintf("ELF: Failed to read entire file (read %d of %lu)\n",
                      bytes_read, file_size);
        kfree(buffer);
        return -1;
    }

    int result = elf_load(buffer, file_size, addr_space, info);
    kfree(buffer);
    return result;
}