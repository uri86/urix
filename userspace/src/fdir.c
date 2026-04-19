/**
 * Licensed under MIT License - URIX project
 * fdir.c - print directory entries of a specific path
 */
#include <urix.h>
#include <auth.h>

int main(int argc, char *argv[])
{
    require_root();
    
    if (argc < 2) {
        println("Usage: fdir <path>");
        return -1;
    }
    
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        println("fdir: Failed to open directory.");
        return -1;
    }
    
    fdir(fd);
    
    close(fd);
    return 0;
}
