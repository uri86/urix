#ifndef LOGO_H
#define LOGO_H

/* The URIX logo in ascii characters. */
static const char *urix_logo[] = {
    "_               _    _   ____   ___ __   __",
    "\\ \\            | |  | | |  _ \\ |_ _|\\ \\ / /",
    " > >           | |  | | | |_) | | |  \\ V / ",
    "/_/  _______   | |__| | |  _ <  | |  / ^ \\ ",
    "    |_______|   \\____/  |_| \\_\\|___|/_/ \\_\\",
};


/*
* prints the urix logo and prints to the screen using the vga buffer.
*/
void print_logo(void);

#endif