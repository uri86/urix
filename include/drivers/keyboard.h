/*
 * Licensed under MIT License - URIX project.
 * keyboard.h - PS/2 Keyboard driver interface.
 * Responsibilities:
 * - Handle keyboard interrupts (IRQ1)
 * - Translate scancodes to ASCII
 * - Track modifier keys (Shift, Ctrl, Alt, Caps Lock)
 * - Provide input buffer for reading keystrokes
 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stddef.h>

/* Keyboard I/O ports */
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_COMMAND_PORT 0x64

/* Special key codes */
#define KEY_ESC 0x1B
#define KEY_BACKSPACE 0x08
#define KEY_TAB 0x09
#define KEY_ENTER 0x0A
#define KEY_CTRL 0x00
#define KEY_SHIFT 0x00
#define KEY_ALT 0x00
#define KEY_CAPS_LOCK 0x00
#define KEY_F1 0x80
#define KEY_F2 0x81
#define KEY_F3 0x82
#define KEY_F4 0x83
#define KEY_F5 0x84
#define KEY_F6 0x85
#define KEY_F7 0x86
#define KEY_F8 0x87
#define KEY_F9 0x88
#define KEY_F10 0x89
#define KEY_F11 0x8A
#define KEY_F12 0x8B
#define KEY_UP 0x90
#define KEY_DOWN 0x91
#define KEY_LEFT 0x92
#define KEY_RIGHT 0x93
#define KEY_PAGE_UP 0x94
#define KEY_PAGE_DOWN 0x95
#define KEY_HOME 0x96
#define KEY_END 0x97
#define KEY_INSERT 0x98
#define KEY_DELETE 0x99

/* Keyboard state flags */
#define KB_SHIFT_L (1 << 0)
#define KB_SHIFT_R (1 << 1)
#define KB_CTRL_L (1 << 2)
#define KB_CTRL_R (1 << 3)
#define KB_ALT_L (1 << 4)
#define KB_ALT_R (1 << 5)
#define KB_CAPS_LOCK (1 << 6)
#define KB_NUM_LOCK (1 << 7)
#define KB_SCROLL_LOCK (1 << 8)

#define KB_SHIFT (KB_SHIFT_L | KB_SHIFT_R)
#define KB_CTRL (KB_CTRL_L | KB_CTRL_R)
#define KB_ALT (KB_ALT_L | KB_ALT_R)

/* Input buffer size */
#define KEYBOARD_BUFFER_SIZE 256

/**
 * keyboard_init - Initialize the keyboard driver
 */
void keyboard_init(void);

/**
 * keyboard_getchar - Read one character from keyboard buffer
 *
 * Returns the next character from the keyboard buffer.
 * If buffer is empty, returns 0.
 *
 * Returns: ASCII character or special key code, 0 if no input
 */
char keyboard_getchar(void);

/**
 * keyboard_getchar_blocking - Read character
 *
 * Waits until a key is pressed and returns it.
 *
 * Returns: ASCII character or special key code
 */
char keyboard_getchar_blocking(void);

/**
 * keyboard_available - Check if input is available
 *
 * Returns: 1 if there are characters in the buffer, 0 otherwise
 */
int keyboard_available(void);

/**
 * keyboard_gets - Read a line of input
 *
 * buf: Buffer to store input
 * size: Maximum size of buffer
 *
 * Reads characters until Enter is pressed or buffer is full.
 * Handles backspace and echoes characters to screen.
 * Null-terminates the result.
 *
 * Returns: Number of characters read (excluding null terminator)
 */
size_t keyboard_gets(char *buf, size_t size);

/**
 * keyboard_get_state - Get current keyboard modifier state
 *
 * Returns: Bitmask of KB_* flags indicating active modifiers
 */
uint16_t keyboard_get_state(void);

/**
 * keyboard_interrupt_handler - Handle keyboard interrupt (IRQ1)
 *
 * Called by IRQ1 handler when keyboard data is available.
 * Processes scancodes and adds characters to input buffer.
 *
 * Note: This is called from interrupt context.
 */
void keyboard_interrupt_handler(void);

#endif /* KEYBOARD_H */