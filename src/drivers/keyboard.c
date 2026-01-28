/*
 * Licensed under MIT License - URIX project.
 * keyboard.c - PS/2 Keyboard driver implementation.
 */

#include <drivers/keyboard.h>
#include <drivers/vga.h>
#include <interrupts/pic.h>
#include <lib/print.h>
#include <lib/string.h>

#define CURSOR_BLOCK 0xDB

/* I/O port access */
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

/* Keyboard state */
static uint16_t keyboard_state = 0;
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile size_t buffer_read = 0;
static volatile size_t buffer_write = 0;

/* QWERTY scancode to ASCII mapping */
static const char scancode_to_ascii[] = {
    0, 0x1B, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0','-','=', '\b','\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o',
    'p', '[', ']', '\n', 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', 'b', 'n',
    'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1', '2',
    '3', '0','.', 0, 0, 0, 0,
};

/* Shifted characters */
static const char scancode_to_ascii_shift[] = {
    0, 0x1B, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P',
    '{', '}', '\n', 0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>',
    '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.', 0, 0,
    0, 0,
};

/* Special scancode sequences (E0 prefix) */
static int escape_sequence = 0;

/**
 * buffer_push - Add character to keyboard buffer
 */
static void buffer_push(char c)
{
    volatile size_t next_write = (buffer_write + 1) % KEYBOARD_BUFFER_SIZE;

    /* Don't overflow buffer */
    if (next_write == buffer_read)
        return;

    keyboard_buffer[buffer_write] = c;

    /* Ensure write completes before updating index */
    __asm__ volatile("" ::: "memory");

    buffer_write = next_write;
}

/**
 * apply_caps_lock - Apply caps lock to alphabetic character
 */
static char apply_caps_lock(char c)
{
    if (keyboard_state & KB_CAPS_LOCK)
    {
        if (c >= 'a' && c <= 'z')
            return c - 32; /* To uppercase */
        else if (c >= 'A' && c <= 'Z')
            return c + 32; /* To lowercase */
    }
    return c;
}

/**
 * keyboard_interrupt_handler - Handle keyboard IRQ (IRQ1)
 */
void keyboard_interrupt_handler(void)
{
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    /* Handle escape sequences (E0 prefix for extended keys) */
    if (scancode == 0xE0)
    {
        escape_sequence = 1;
        return;
    }

    /* Check if this is a key release (bit 7 set) */
    int released = (scancode & 0x80) != 0;
    uint8_t key = scancode & 0x7F;

    /* Handle special keys and modifiers */
    if (escape_sequence)
    {
        escape_sequence = 0;

        /* Extended key handling */
        switch (key)
        {
        case 0x1D: /* Right Ctrl */
            if (released)
                keyboard_state &= ~KB_CTRL_R;
            else
                keyboard_state |= KB_CTRL_R;
            return;

        case 0x38: /* Right Alt */
            if (released)
                keyboard_state &= ~KB_ALT_R;
            else
                keyboard_state |= KB_ALT_R;
            return;

        case 0x48: /* Up arrow */
            if (!released)
                buffer_push(KEY_UP);
            return;
        case 0x50: /* Down arrow */
            if (!released)
                buffer_push(KEY_DOWN);
            return;
        case 0x4B: /* Left arrow */
            if (!released)
                buffer_push(KEY_LEFT);
            return;
        case 0x4D: /* Right arrow */
            if (!released)
                buffer_push(KEY_RIGHT);
            return;
        case 0x49: /* Page Up */
            if (!released)
                buffer_push(KEY_PAGE_UP);
            return;
        case 0x51: /* Page Down */
            if (!released)
                buffer_push(KEY_PAGE_DOWN);
            return;
        case 0x47: /* Home */
            if (!released)
                buffer_push(KEY_HOME);
            return;
        case 0x4F: /* End */
            if (!released)
                buffer_push(KEY_END);
            return;
        case 0x52: /* Insert */
            if (!released)
                buffer_push(KEY_INSERT);
            return;
        case 0x53: /* Delete */
            if (!released)
                buffer_push(KEY_DELETE);
            return;
        }

        return;
    }

    /* Handle modifier keys */
    switch (key)
    {
    case 0x2A: /* Left Shift */
        if (released)
            keyboard_state &= ~KB_SHIFT_L;
        else
            keyboard_state |= KB_SHIFT_L;
        return;

    case 0x36: /* Right Shift */
        if (released)
            keyboard_state &= ~KB_SHIFT_R;
        else
            keyboard_state |= KB_SHIFT_R;
        return;

    case 0x1D: /* Left Ctrl */
        if (released)
            keyboard_state &= ~KB_CTRL_L;
        else
            keyboard_state |= KB_CTRL_L;
        return;

    case 0x38: /* Left Alt */
        if (released)
            keyboard_state &= ~KB_ALT_L;
        else
            keyboard_state |= KB_ALT_L;
        return;

    case 0x3A: /* Caps Lock */
        if (!released)
            keyboard_state ^= KB_CAPS_LOCK; /* Toggle */
        return;

    case 0x45: /* Num Lock */
        if (!released)
            keyboard_state ^= KB_NUM_LOCK; /* Toggle */
        return;

    case 0x46: /* Scroll Lock */
        if (!released)
            keyboard_state ^= KB_SCROLL_LOCK; /* Toggle */
        return;
    }

    /* Ignore key releases for regular keys */
    if (released)
        return;

    /* Handle function keys */
    if (key >= 0x3B && key <= 0x44) /* F1-F10 */
    {
        buffer_push(KEY_F1 + (key - 0x3B));
        return;
    }
    if (key == 0x57) /* F11 */
    {
        buffer_push(KEY_F11);
        return;
    }
    if (key == 0x58) /* F12 */
    {
        buffer_push(KEY_F12);
        return;
    }

    /* Convert scancode to ASCII */
    char ascii = 0;

    if (key < sizeof(scancode_to_ascii))
    {
        /* Check for Shift or Caps Lock */
        int shifted = (keyboard_state & KB_SHIFT) != 0;

        if (shifted)
        {
            ascii = scancode_to_ascii_shift[key];
        }
        else
        {
            ascii = scancode_to_ascii[key];

            /* Apply caps lock to letters only */
            if (ascii >= 'a' && ascii <= 'z')
                ascii = apply_caps_lock(ascii);
        }

        /* Handle Ctrl combinations */
        if ((keyboard_state & KB_CTRL) && ascii >= 'a' && ascii <= 'z')
        {
            ascii = ascii - 'a' + 1; /* Ctrl+A = 0x01, etc. */
        }
        else if ((keyboard_state & KB_CTRL) && ascii >= 'A' && ascii <= 'Z')
        {
            ascii = ascii - 'A' + 1;
        }

        if (ascii != 0)
        {
            buffer_push(ascii);
        }
    }
}

void keyboard_init(void)
{
    debug_kprintf("Initializing keyboard driver...\n");

    keyboard_state = 0;
    buffer_read = 0;
    buffer_write = 0;
    escape_sequence = 0;

    /* Clear keyboard buffer manually to avoid memset issues */
    for (int i = 0; i < KEYBOARD_BUFFER_SIZE; i++)
        keyboard_buffer[i] = 0;

    /* Flush keyboard buffer */
    int timeout = 10000;
    while ((inb(KEYBOARD_STATUS_PORT) & 1) && timeout--)
    {
        inb(KEYBOARD_DATA_PORT);
    }

    debug_kprintf("Keyboard driver initialized\n");
}

char keyboard_getchar(void)
{
    /* Disable interrupts while accessing buffer */
    __asm__ volatile("cli");

    if (buffer_read == buffer_write)
    {
        __asm__ volatile("sti");
        return 0; /* Buffer empty */
    }

    char c = keyboard_buffer[buffer_read];

    /* Ensure read completes before updating index */
    __asm__ volatile("" ::: "memory");

    buffer_read = (buffer_read + 1) % KEYBOARD_BUFFER_SIZE;

    __asm__ volatile("sti");
    return c;
}

char keyboard_getchar_blocking(void)
{
    while (buffer_read == buffer_write)
    {
        __asm__ volatile("hlt"); /* Wait for interrupt */
    }

    return keyboard_getchar();
}

int keyboard_available(void)
{
    volatile size_t read = buffer_read;
    volatile size_t write = buffer_write;
    return read != write;
}

size_t keyboard_gets(char *buf, size_t size)
{
    if (!buf || size == 0)
        return 0;

    size_t pos = 0;

    /* Null terminate in case we get nothing */
    buf[0] = '\0';

    while (pos < size - 1)
    {
        // add visual cursor using the ascii block (code 219)
        console_putchar(CURSOR_BLOCK);
        char c = keyboard_getchar_blocking();
        console_putchar('\b');
        if (c == '\n' || c == '\r')
        {
            /* Echo newline */
            console_putchar('\n');
            break;
        }
        else if (c == '\b' || c == 0x7F) /* Backspace or Delete */
        {
            if (pos > 0)
            {
                pos--;
                /* Erase character on screen */
                console_putchar('\b');
            }
        }
        else if (c >= 32 && c <= 126) /* Printable ASCII */
        {
            buf[pos++] = c;
            console_putchar(c); /* Echo character */
        }
        /* Ignore other characters (like function keys, arrows, etc.) */
    }

    buf[pos] = '\0';
    return pos;
}

uint16_t keyboard_get_state(void)
{
    return keyboard_state;
}