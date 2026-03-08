/**
 * Licensed under MIT License - URIX project
 * cp.c - Copy a file
 * Usage: cp <source> <dest>
 */
#include <urix.h>

#define BUF_SIZE 512

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        println("Usage: cp <source> <dest>");
        exit(1);
    }

    const char *src = argv[1];
    const char *dest = argv[2];

    int src_fd = open(src, O_RDONLY);
    if (src_fd < 0)
    {
        print("cp: cannot open source '");
        print(src);
        println("'");
        exit(1);
    }

    int dst_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC);
    if (dst_fd < 0)
    {
        print("cp: cannot create destination '");
        print(dest);
        println("'");
        close(src_fd);
        exit(1);
    }

    char buf[BUF_SIZE];
    int bytes;
    int ret = 0;

    while ((bytes = read(src_fd, buf, sizeof(buf))) > 0)
    {
        int written = write(dst_fd, buf, bytes);
        if (written != bytes)
        {
            print("cp: write error to '");
            print(dest);
            println("'");
            ret = 1;
            break;
        }
    }

    if (bytes < 0)
    {
        print("cp: read error from '");
        print(src);
        println("'");
        ret = 1;
    }

    close(src_fd);
    close(dst_fd);
    return ret;
}