#include <stdint.h>
#include <stddef.h>
#include <lib/print.h>
#include <lib/utils.h>
#include <lib/logo.h>
#include <lib/string.h>
#include <memory/physical/pmm.h>
#include <process/process.h>
#include <interrupts/idt.h>
#include <interrupts/pic.h>
#include <memory/vmm.h>

#define KERNEL_BASE 0xFFFF800000000000ULL
#define IDENTITY_WINDOW_BYTES (1ULL << 30)
#define KERNEL_STACK_PAGES 4
#define KERNEL_STACK_SIZE (KERNEL_STACK_PAGES * PAGE_SIZE)
#define KERNEL_STACK_VA_OFFSET (0x100000)
#define KERNEL_STACK_TOP (KERNEL_BASE + KERNEL_STACK_VA_OFFSET + KERNEL_STACK_SIZE)

extern char _kernel_start;
extern char _kernel_end;

static inline uint64_t align_up(uint64_t x, uint64_t a) { return (x + a - 1) & ~(a - 1); }

static inline uint64_t read_cr3(void)
{
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

// Forward declaration for the high-half kernel function
void kernel_after_remap(void);

void kernel_main(uint64_t mb_info_addr)
{
    uint8_t *cmd = NULL;
    multiboot_size_tag *tag = (multiboot_size_tag *)(uintptr_t)mb_info_addr;
    multiboot_tag *t = (multiboot_tag *)((uint8_t *)mb_info_addr + 8);

    while (t->type != MULTIBOOT_TAG_TYPE_END)
    {
        if (t->type == MULTIBOOT_TAG_TYPE_BCL)
        {
            multiboot_tag_bcl *cmd_tag = (multiboot_tag_bcl *)t;
            cmd = cmd_tag->string;
        }
        t = (multiboot_tag *)((uint8_t *)t + ((t->size + 7) & ~7));
    }

    if (cmd && strcmp((char *)cmd, "debug") == 0)
    {
        debug_mode = 1;
    }

    clear_screen();
    print_logo();

    /* Initialize IDT */
    kprintf("Initializing IDT...\n");
    idt_init();

    /* Initialize PIC */
    kprintf("Initializing PIC...\n");
    pic_init(0x20, 0x28);

    /* Mask all IRQs except timer */
    for (int i = 1; i < 16; i++)
    {
        pic_mask_irq(i);
    }
    pic_unmask_irq(0);

    /* Initialize PMM - this also creates identity mapping and switches CR3 */
    debug_delay_ms = 150;
    kprintf("Initializing PMM...\n");
    pmm_init(tag);
    pmm_print_stats();

    kprintf("\nPMM initialized with identity mapping active.\n");
    kprintf("Current CR3: %llx\n", read_cr3());

    /* Now get the current PML4 (created by PMM) */
    uint64_t current_pml4 = read_cr3();

    /* Calculate kernel boundaries */
    uint64_t kernel_phys_start = (uint64_t)&_kernel_start;
    uint64_t kernel_phys_end = (uint64_t)&_kernel_end;
    uint64_t kernel_size = 0;

    if (kernel_phys_end > kernel_phys_start)
        kernel_size = align_up(kernel_phys_end - kernel_phys_start, PAGE_SIZE);

    if (kernel_size == 0)
    {
        kprintf("kernel_main: ERROR - kernel size is 0\n");
        for (;;)
            __asm__ volatile("hlt");
    }

    kprintf("Kernel physical: [%llx - %llx] (%llu KB)\n",
            kernel_phys_start, kernel_phys_end, kernel_size / 1024);

    /* Map kernel into higher half using the EXISTING PML4 from PMM */
    uint64_t kernel_va_start = KERNEL_BASE;

    kprintf("Mapping kernel to high-half: %llx -> %llx\n",
            kernel_phys_start, kernel_va_start);

    if (vmm_map_range(current_pml4, kernel_va_start, kernel_phys_start, kernel_size, PAGE_RW) != 0)
    {
        kprintf("kernel_main: ERROR - failed to map kernel to high-half\n");
        for (;;)
            __asm__ volatile("hlt");
    }

    kprintf("Kernel mapped to high-half successfully\n");

    /* Allocate and map kernel stack in high-half */
    uint64_t stack_va = KERNEL_BASE + KERNEL_STACK_VA_OFFSET;

    kprintf("Allocating kernel stack at virtual address %llx\n", stack_va);

    for (int i = 0; i < KERNEL_STACK_PAGES; ++i)
    {
        uint64_t frame = pmm_alloc_frame();
        if (!frame)
        {
            kprintf("kernel_main: ERROR - failed to allocate stack frame %d\n", i);
            for (;;)
                __asm__ volatile("hlt");
        }

        /* Zero the frame via identity mapping */
        memset((void *)(uintptr_t)frame, 0, PAGE_SIZE);

        /* Map it to high-half stack address */
        if (vmm_map_page(current_pml4, stack_va + (i * PAGE_SIZE), frame, PAGE_RW) != 0)
        {
            kprintf("kernel_main: ERROR - failed to map stack page %d\n", i);
            for (;;)
                __asm__ volatile("hlt");
        }
    }

    kprintf("Kernel stack mapped: [%llx - %llx]\n",
            stack_va, stack_va + KERNEL_STACK_SIZE);

    /* Flush TLB to ensure new mappings are active */
    kprintf("Flushing TLB...\n");
    __asm__ volatile("mov %%cr3, %%rax; mov %%rax, %%cr3" ::: "rax", "memory");

    /* Now jump to high-half kernel */
    kprintf("\n=== Jumping to High-Half Kernel ===\n");

    /* Calculate high-half address of kernel_after_remap */
    uint64_t kernel_phys_func = (uint64_t)kernel_after_remap;
    uint64_t offset_in_kernel = kernel_phys_func - kernel_phys_start;
    uint64_t high_half_func = KERNEL_BASE + offset_in_kernel;

    kprintf("kernel_after_remap: phys=%llx virt=%llx\n",
            kernel_phys_func, high_half_func);

    __asm__ volatile(
        "mov %0, %%rsp\n\t"    // Set new stack
        "xor %%rbp, %%rbp\n\t" // Clear frame pointer
        "jmp *%1\n\t"          // Jump to high-half
        :
        : "r"(KERNEL_STACK_TOP),
          "r"(high_half_func)
        : "memory");

    for (;;)
        __asm__ volatile("hlt");
}

void kernel_after_remap(void)
{
    kprintf("kernel_after_remap: running with new page tables and kernel stack\n");

    /* NOW it is safe to enable interrupts */
    kprintf("Enabling interrupts...\n");
    __asm__ volatile("sti");

    /* Test: print first few bytes of kernel */
    volatile uint8_t *p = (uint8_t *)(uintptr_t)KERNEL_BASE;
    kprintf("kernel_after_remap: first kernel bytes: %x %x %x %x\n",
            (unsigned)p[0], (unsigned)p[1], (unsigned)p[2], (unsigned)p[3]);

    uint64_t counter = 0;
    for (;;)
    {
        if (counter % 100000000 == 0)
        {
            kprintf("."); // Heartbeat
        }
        counter++;
        __asm__ volatile("pause");
    }
}