/**
 * Licensed under MIT License - URIX project
 * rndrbn.c - Round Robin tester in userspace.
 */

#include <urix.h>

int main(void)
{
    fork();

    int pid = getpid();
    for (int i = 0; i < 500; i++)
    {
        print("I am pid ");

        /* Print PID */
        if (pid >= 1000)
            putchar('0' + (pid / 1000));
        if (pid >= 100)
            putchar('0' + ((pid / 100) % 10));
        if (pid >= 10)
            putchar('0' + ((pid / 10) % 10));
        putchar('0' + (pid % 10));

        print(" on iteration: ");

        if (i >= 1000)
            putchar('0' + (i / 1000));
        if (i >= 100)
            putchar('0' + ((i / 100) % 10));
        if (i >= 10)
            putchar('0' + ((i / 10) % 10));

        putchar('0' + (i % 10));

        putchar('\n');
    }

    exit(0);
}