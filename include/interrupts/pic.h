/*
 * Licensed under MIT License - URIX project.
 * pic.h - Programmable Interrupt Controller interface.
 * Responsibilities:
 *  - Initialize and remap the PICs
 *  - Send EOI (End of Interrupt) signals
 *  - Mask/unmask specific IRQ lines
 */

#ifndef PIC_H
#define PIC_H

#include <stdint.h>

/* PIC I/O ports */
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

/* PIC initialization commands */
#define ICW1_ICW4 0x01      /* ICW4 needed */
#define ICW1_SINGLE 0x02
#define ICW1_INTERVAL4 0x04
#define ICW1_LEVEL 0x08
#define ICW1_INIT 0x10

#define ICW4_8086 0x01
#define ICW4_AUTO 0x02       /* Auto EOI */
#define ICW4_BUF_SLAVE 0x08
#define ICW4_BUF_MASTER 0x0C
#define ICW4_SFNM 0x10

/* EOI command */
#define PIC_EOI 0x20

/**
 * pic_init - Initialize and remap the PICs
 *
 * Remaps PIC1 to start at offset1
 * Remaps PIC2 to start at offset2
 * This prevents conflicts with CPU exceptions (0-31)
 */
void pic_init(uint8_t offset1, uint8_t offset2);

/**
 * pic_send_eoi - Send End of Interrupt signal
 *
 * irq: IRQ number (0-15)
 */
void pic_send_eoi(uint8_t irq);

/**
 * pic_mask_irq - Disable a specific IRQ line
 *
 * irq: IRQ number to mask (0-15)
 */
void pic_mask_irq(uint8_t irq);

/**
 * pic_unmask_irq - Enable a specific IRQ line
 *
 * irq: IRQ number to unmask (0-15)
 */
void pic_unmask_irq(uint8_t irq);

/**
 * pic_disable - Disable all IRQs on both PICs
 */
void pic_disable(void);

#endif /* PIC_H */