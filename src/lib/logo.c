/*
 * Licensed under MIT License - URIX project.
 * logo.c - ASCII art logo rendering for URIX.
 * Responsibilities:
 *  - store the URIX logo as ASCII strings
 *  - print the logo to screen using VGA text mode
 *  - manage color changes before and after logo output
 * Notes:
 *  - uses kprintf() from print.c
 *  - colors default to cyan for logo and green after
 */

#include <lib/print.h>
#include <lib/logo.h>

/* The URIX logo in ascii characters. */
static const char *urix_logo[] = {
    "_               _    _   ____   ___ __   __",
    "\\ \\            | |  | | |  _ \\ |_ _|\\ \\ / /",
    " > >           | |  | | | |_) | | |  \\ V / ",
    "/_/  _______   | |__| | |  _ <  | |  / ^ \\ ",
    "    |_______|   \\____/  |_| \\_\\|___|/_/ \\_\\",
};

void print_logo(void)
{
    size_t lines = sizeof(urix_logo) / sizeof(urix_logo[0]);
    set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);

    for (size_t i = 0; i < lines; i++)
        kprintf("%s\n", urix_logo[i]);
    
    set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
}