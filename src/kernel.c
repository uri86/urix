/*
 * kernel.c
 *
 * Responsibilities:
 *  - Initialize the physical memory manager (pmm)
 *
 * Notes:
 *  - GRUB passes the Multiboot2 info pointer as the first argument to
 *    kernel_main (RDI) because boot.S calls `kernel_main(mb_info_ptr)`.
 *  - boot.S creates identity mapping for low memory (first 1 GiB),
 *    so we can read/write low memory here. See boot.S for details.
 */

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#include <lib/print.h>
#include <lib/logo.h> 
#include <memory/physical/pmm.h>


void kernel_main(uint64_t mb_info_addr)
{
    multiboot_size_tag *tag = (multiboot_size_tag *)(uintptr_t)(mb_info_addr);
    clear_screen();
    print_logo();
    pmm_init(tag);
    uint64_t frame = pmm_alloc_frame();
    kprintf("Free frames: %llx\n", pmm_get_free_frames);
    uint64_t frame2 = pmm_alloc_frame();
    kprintf("Frames: 1: %llx 2: %llx total free: %llx\n", frame, frame2, pmm_get_free_frames());
    pmm_free_frame(frame2);
    kprintf("Free frames: %llx\n", pmm_get_free_frames);
    pmm_free_frame(frame);

    /* Halt CPU: change this later to run more kernel code */
    for (;;)
    {
        __asm__ volatile("hlt");
    }
}
