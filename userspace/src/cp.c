/**
 * Licensed under MIT License - URIX project
 * cp.c - Copy a file
 * Usage: cp <source> <dest>
 */
#include <urix.h>

#define BUF_SIZE 512

char *basename(const char *path) {
    const char *p = path + strlen(path);
    while (p > path && *(p-1) != '/') p--;
    return (char *)p;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        println("Usage: cp <source> <dest>");
        exit(1);
    }

    const char *src = argv[1];
    char dest_path[256];
    strcpy(dest_path, argv[2]);
    const char *dest = dest_path;

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
        // Try as directory
        char new_dest[256];
        strcpy(new_dest, dest);
        size_t len = strlen(new_dest);
        if (len > 0 && new_dest[len - 1] != '/')
        {
            new_dest[len] = '/';
            new_dest[len + 1] = '\0';
        }
        const char *base = basename(src);
        if (strlen(new_dest) + strlen(base) >= sizeof(new_dest))
        {
            print("cp: destination path too long");
            close(src_fd);
            exit(1);
        }
        strcpy(new_dest + strlen(new_dest), base);
        dst_fd = open(new_dest, O_WRONLY | O_CREAT | O_TRUNC);
        if (dst_fd < 0)
        {
            print("cp: cannot create destination '");
            print(dest);
            println("'");
            close(src_fd);
            exit(1);
        }
        dest = new_dest;
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