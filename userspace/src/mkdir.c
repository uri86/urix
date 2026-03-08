/**
 * Licensed under MIT License - URIX project
 * mkdir.c - Make directories
 * Usage: mkdir [-p] <dir> [dir2 ...]
 */
#include <urix.h>

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        println("Usage: mkdir [-p] <dir> [dir2 ...]");
        exit(1);
    }

    int flag_p = 0;
    int ret = 0;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            for (int j = 1; argv[i][j]; j++)
            {
                if (argv[i][j] == 'p')
                    flag_p = 1;
                else
                {
                    print("mkdir: unknown flag: -");
                    putchar(argv[i][j]);
                    print("\n");
                    exit(1);
                }
            }
            continue;
        }

        /* mode 0755 — kernel ignores it for now */
        int r = mkdir(argv[i], 0755);
        if (r < 0)
        {
            if (flag_p)
            {
                /* -p: silently ignore "already exists" */
                continue;
            }
            print("mkdir: cannot create directory '");
            print(argv[i]);
            println("'");
            ret = 1;
        }
    }

    return ret;
}