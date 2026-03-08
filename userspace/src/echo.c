/**
 * Licensed under MIT License - URIX project
 * echo.c - Print arguments to stdout
 * Usage: echo [-n] [string ...]
 */
#include <urix.h>

int main(int argc, char **argv)
{
    int flag_n = 0;
    int first_arg = 1;

    /* Check if first arg is -n */
    if (argc >= 2 && argv[1][0] == '-' && argv[1][1] == 'n' && argv[1][2] == '\0')
    {
        flag_n = 1;
        first_arg = 2;
    }

    for (int i = first_arg; i < argc; i++)
    {
        if (i > first_arg)
            print(" ");
        print(argv[i]);
    }

    if (!flag_n)
        print("\n");

    return 0;
}