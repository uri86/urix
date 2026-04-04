/**
 * Licensed under MIT License - URIX project
 * prntpcb.c - prints the PCB of a prcoess
 */
#include <urix.h>
#include <string.h>
#include "auth.h"

void main(int argc, char **argv)
{
    require_root();
    if (argc < 2)
    {
        println("Usage: prntpcb <pid>");
        exit(1);
    }
    print_pcb(atoi(argv[1]));
}