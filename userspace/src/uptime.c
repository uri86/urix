/**
 * Licensed under MIT License - URIX project
 * uptime.c - Command to display system uptime
 */
#include <urix.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    long secs = uptime();
    char buf[64];
    snprintf(buf, sizeof(buf), "Uptime: %ld seconds\n", secs);
    print(buf);

    return 0;
}
