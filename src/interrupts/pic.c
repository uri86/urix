/*
 * Licensed under MIT License - URIX project.
 * pic.c - Programmable Interrupt Controller implementation.
 * Responsibilities:
 * - Configure the 8259 PICs for x86-64 mode
 * - Remap IRQs to avoid conflicts with CPU exceptions
 * - Handle EOI signaling
 * - Manage IRQ masking
 */

#include <interrupts/pic.h>
#include <stdint.h>

/* I/O port access functions */
static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* I/O wait - small delay for old hardware */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

void pic_init(uint8_t offset1, uint8_t offset2)
{
    /* Start initialization sequence (ICW1) */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    /* Set vector offsets (ICW2) */
    outb(PIC1_DATA, offset1);
    io_wait();
    outb(PIC2_DATA, offset2);
    io_wait();

    /* Tell master PIC there's a slave at IRQ2 (ICW3) */
    outb(PIC1_DATA, 4);
    io_wait();

    /* Tell slave PIC its cascade identity (ICW3) */
    outb(PIC2_DATA, 2);
    io_wait();

    /* Set 8086/88 mode (ICW4) */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* * Mask all interrupts (0xFF) on both PICs to prevent spurious IRQs.
     * We will explicitly unmask specific IRQs (like timer) later in kernel_main.
     */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_send_eoi(uint8_t irq)
{
    /* If IRQ came from slave PIC, send EOI to both */
    if (irq >= 8)
    {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    /* Always send EOI to master */
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_mask_irq(uint8_t irq)
{
    uint16_t port;
    uint8_t value;

    if (irq < 8)
    {
        port = PIC1_DATA;
    }
    else
    {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = inb(port) | (1 << irq);
    outb(port, value);
}

void pic_unmask_irq(uint8_t irq)
{
    uint16_t port;
    uint8_t value;

    if (irq < 8)
    {
        port = PIC1_DATA;
    }
    else
    {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

void pic_disable(void)
{
    /* Mask all IRQs */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}