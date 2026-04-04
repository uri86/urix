/**
 * Licensed under MIT License - URIX project
 * pmmstat.c - print physical memory stats
 */
#include "urix.h"
#include "auth.h"

int main()
{
    require_root();
    pmmstat();
    return 0;
}
