/**
 * Licensed under MIT License - URIX project
 * kmlcstat.c - print kernel allocator stats
 */
#include <urix.h>
#include <auth.h>

int main()
{
    require_root();
    kmlcstat();
    return 0;
}
