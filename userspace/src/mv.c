/**
 * Licensed under MIT License - URIX project
 * mv.c - Move (rename) a file
 * Usage: mv <source> <dest>
 */
#include <urix.h>

#define BUF_SIZE 512

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        println("Usage: mv <source> <dest>");
        exit(1);
    }

    const char *src = argv[1];
    const char *dest = argv[2];

    int src_fd = open(src, O_RDONLY);
    if (src_fd < 0)
    {
        print("mv: cannot open '");
        print(src);
        println("'");
        exit(1);
    }

    int dst_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC);
    if (dst_fd < 0)
    {
        print("mv: cannot create '");
        print(dest);
        println("'");
        close(src_fd);
        exit(1);
    }

    char buf[BUF_SIZE];
    int bytes;
    int ok = 1;

    while ((bytes = read(src_fd, buf, sizeof(buf))) > 0)
    {
        if (write(dst_fd, buf, bytes) != bytes)
        {
            println("mv: write error");
            ok = 0;
            break;
        }
    }

    close(src_fd);
    close(dst_fd);

    if (!ok)
    {
        unlink(dest);
        exit(1);
    }

    if (unlink(src) < 0)
    {
        print("mv: could not remove source '");
        print(src);
        println("'");
        exit(1);
    }

    return 0;
}