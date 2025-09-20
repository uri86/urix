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
}