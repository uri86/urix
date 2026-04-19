/**
 * Licensed under MIT License - URIX project
 * fsstat.c - print active filesystem stats
 */
#include <urix.h>
#include <auth.h>

int main()
{
    require_root();
    fsstat();
    return 0;
}
