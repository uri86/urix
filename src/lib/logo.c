#include <lib/print.h>
#include <lib/logo.h>

void print_logo(void)
{
    size_t lines = sizeof(urix_logo) / sizeof(urix_logo[0]);
    set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);

    for (size_t i = 0; i < lines; i++)
        kprintf("%s\n", urix_logo[i]);
}