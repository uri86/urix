/*
 * kernel.c - URIX 64-bit kernel
 * Main kernel entry point and basic VGA text output
 */

#include <stdint.h>
#include <stddef.h>

// VGA text mode constants
#define VGA_WIDTH 80       // width of screen in characters
#define VGA_HEIGHT 25      // height of screen in lines
#define VGA_MEMORY 0xB8000 // memory-mapped address of VGA text buffer

// VGA color constants
typedef enum
{
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
} vga_color_t;

// terminal state variables
static size_t terminal_row = 0;    // current row position
static size_t terminal_column = 0; // current column position
static uint8_t terminal_color = 0; // current color
static volatile uint16_t *terminal_buffer = (volatile uint16_t *)VGA_MEMORY;

// helper function to combine foreground and background colors into one byte
static inline uint8_t vga_entry_color(vga_color_t fg, vga_color_t bg)
{
    return fg | bg << 4; // lower 4 bits = foreground, upper 4 bits = background
}

// helper function to combine character and color into a 16-bit VGA entry
static inline uint16_t vga_entry(unsigned char uc, uint8_t color)
{
    return (uint16_t)uc | (uint16_t)color << 8;
}

// simple string length function (like strlen in stdlib)
static size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

// initialize terminal: clear screen and reset cursor
static void terminal_initialize(void)
{
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    // fill entire screen with spaces in the default color
    for (size_t y = 0; y < VGA_HEIGHT; y++)
    {
        for (size_t x = 0; x < VGA_WIDTH; x++)
        {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
}

// change current text color
static void terminal_setcolor(uint8_t color)
{
    terminal_color = color;
}

// put a single character 'c' at (x, y) with given color
static void terminal_putentryat(char c, uint8_t color, size_t x, size_t y)
{
    const size_t index = y * VGA_WIDTH + x;
    terminal_buffer[index] = vga_entry(c, color);
}

// scroll terminal up one line when bottom is reached
static void terminal_scroll(void)
{
    // move all lines up by copying each line to the previous line
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
    {
        for (size_t x = 0; x < VGA_WIDTH; x++)
        {
            terminal_buffer[y * VGA_WIDTH + x] = terminal_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }

    // clear the last line to blank spaces
    for (size_t x = 0; x < VGA_WIDTH; x++)
    {
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    }
}

// put a single character at the current cursor position
// handles '\n', '\r', '\t', wrapping, and scrolling
static void terminal_putchar(char c)
{
    if (c == '\n') // newline: move to start of next row
    {
        terminal_column = 0;
        terminal_row++;
    }
    else if (c == '\r') // carriage return: move to start of current row
    {
        terminal_column = 0;
    }
    else if (c == '\t') // tab: advance to next 8-character boundary
    {
        terminal_column = (terminal_column + 8) & ~7;
    }
    else // normal character
    {
        terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
        terminal_column++;
    }

    // wrap lines if end of row reached
    if (terminal_column >= VGA_WIDTH)
    {
        terminal_column = 0;
        terminal_row++;
    }

    // scroll if bottom of screen reached
    if (terminal_row >= VGA_HEIGHT)
    {
        terminal_scroll();
        terminal_row = VGA_HEIGHT - 1;
    }
}

// write a buffer of characters to terminal
static void terminal_write(const char *data, size_t size)
{
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

// write null-terminated string to terminal
static void terminal_writestring(const char *data)
{
    terminal_write(data, strlen(data));
}

// print an unsigned 64-bit integer in decimal
static void print_uint64(uint64_t value)
{
    if (value == 0)
    {
        terminal_putchar('0');
        return;
    }

    char buffer[21]; // enough to store 64-bit number
    int pos = 0;

    // convert number to string in reverse
    while (value > 0)
    {
        buffer[pos++] = '0' + (value % 10);
        value /= 10;
    }

    // print digits in correct order
    for (int i = pos - 1; i >= 0; i--)
    {
        terminal_putchar(buffer[i]);
    }
}

// print an unsigned 64-bit integer in hexadecimal (prefixed with 0x)
static void print_hex(uint64_t value)
{
    terminal_writestring("0x");

    if (value == 0)
    {
        terminal_putchar('0');
        return;
    }

    char buffer[17]; // enough for 64-bit hex
    int pos = 0;

    // convert number to hex string in reverse
    while (value > 0)
    {
        int digit = value % 16;
        buffer[pos++] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
        value /= 16;
    }

    // print digits in correct order
    for (int i = pos - 1; i >= 0; i--)
    {
        terminal_putchar(buffer[i]);
    }
}

// main kernel entry point, called from boot.S after entering 64-bit mode
void kernel_main(void)
{
    // initialize the VGA text terminal
    terminal_initialize();

    // print welcome message in light green
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("Welcome to URIX 64-bit!\n");

    // print confirmation message in white
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("Kernel successfully loaded in 64-bit mode.\n\n");

    // print system information
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
    terminal_writestring("System Information:\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("- Architecture: x86_64\n");
    terminal_writestring("- Kernel: URIX\n");
    terminal_writestring("- VGA Text Mode: 80x25\n");

    // show VGA memory buffer address to demonstrate direct hardware access
    terminal_writestring("- VGA Buffer: ");
    print_hex((uint64_t)VGA_MEMORY);
    terminal_putchar('\n');

    // show kernel stack address (prove 64-bit stack is active)
    terminal_writestring("- Kernel Stack: ");
    uint64_t stack_addr;
    __asm__ volatile("mov %%rsp, %0" : "=r"(stack_addr)); // read 64-bit RSP
    print_hex(stack_addr);
    terminal_putchar('\n');

    // show that 64-bit arithmetic works
    terminal_writestring("- 64-bit test value: ");
    print_uint64(0x1234567890ABCDEF);
    terminal_putchar('\n');

    // print final message and halt
    terminal_setcolor(vga_entry_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\nKernel is running. System halted.\n");

    // infinite loop halts CPU to prevent running into invalid memory
    for (;;)
    {
        __asm__ volatile("hlt"); // halt instruction, reduces CPU usage
    }
}
