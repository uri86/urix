/*
 * Licensed under MIT License - URIX project.
 * logo.h - URIX ASCII logo printer header.
 * Responsibilities:
 *  - declare print_logo() for displaying the URIX logo
 * Notes:
 *  - logo is defined in logo.c
 *  - relies on VGA text mode for output
 */

#ifndef LOGO_H
#define LOGO_H

/*
* prints the urix logo and prints to the screen using the vga buffer.
*/
void print_logo(void);

#endif