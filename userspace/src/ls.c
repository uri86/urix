/**
 * Licensed under MIT License - URIX poroject
 * ls.c - List directory contents
 * Usage: ls [-l] [-a] [path]
 */
#include <urix.h>

static void print_uint(unsigned long long n)
{
    if (n == 0)
    {
        print("0");
        return;
    }

    char buf[24];
    char out[24];
    int i = 0, j = 0;

    while (n)
    {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }

    while (i--)
    {
        out[j++] = buf[i];
    }
    out[j] = '\0';

    print(out);
}

static void pad_right(const char *s, int width)
{
    int len = strlen(s);
    print(s);

    if (len < width)
    {
        char spaces[64];
        int pad = width - len;
        if (pad > 63)
            pad = 63;

        for (int i = 0; i < pad; i++)
        {
            spaces[i] = ' ';
        }
        spaces[pad] = '\0';

        print(spaces);
    }
}

int main(int argc, char **argv)
{
    int flag_l = 0;
    int flag_a = 0;
    const char *path = ".";
    char flags[2];
    flags[1] = '\0';
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            for (int j = 1; argv[i][j]; j++)
            {
                if (argv[i][j] == 'l')
                    flag_l = 1;
                else if (argv[i][j] == 'a')
                    flag_a = 1;
                else
                {
                    print("ls: unknown flag: -");
                    flags[0] = argv[i][j];
                    print(flags);
                    print("\n");
                    exit(1);
                }
            }
        }
        else
        {
            path = argv[i];
        }
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        print("ls: cannot open '");
        print(path);
        println("': no such file or directory");
        exit(1);
    }

    dirent_t entry;
    int count = 0;

    while (readdir(fd, &entry) == 0)
    {
        if (!flag_a)
        {
            if (entry.name[0] == '.' &&
                (entry.name[1] == '\0' ||
                 (entry.name[1] == '.' && entry.name[2] == '\0')))
                continue;
        }

        if (flag_l)
        {
            if (entry.type == 2)
                print("d ");
            else
                print("- ");

            pad_right(entry.name, 24);

            print("  inode=");
            print_uint(entry.inode);
            print("\n");
        }
        else
        {
            print(entry.name);
            if (entry.type == 2)
                print("/");
            print("\n");
        }
        count++;
    }

    if (count == 0)
        println("(empty)");

    close(fd);
    return 0;
}