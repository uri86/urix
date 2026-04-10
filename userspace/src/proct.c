/**
 * Licensed under MIT License - URIX project
 * proct.c - print process table
 */
#include <urix.h>
#include <auth.h>

int main()
{
    require_root();
    proct();
    return 0;
}
