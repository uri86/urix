/**
 * Licensed under MIT License - URIX project
 * fblk.c - print blocks array map of a specific file
 */
#include <urix.h>
#include <auth.h>

int main(int argc, char *argv[])
{
    require_root();
    
    if (argc < 2) {
        println("Usage: fblk <path>");
        return -1;
    }
    
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        println("fblk: Failed to open file.");
        return -1;
    }
    
    fblk(fd);
    
    close(fd);
    return 0;
}
