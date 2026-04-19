/**
 * Licensed under MIT License - URIX project
 * vfstrace.c - trace VFS path resolution
 */
#include <urix.h>
#include <auth.h>

int main(int argc, char *argv[])
{
    require_root();

    if (argc < 2) {
        println("Usage: vfstrace <path>");
        return -1;
    }

    vfstrace(argv[1]);
    return 0;
}
