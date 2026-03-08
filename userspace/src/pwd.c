/**
 * Licensed under MIT License - URIX project
 * pwd.c - Print working directory
 */
#include <urix.h>

int main(void)
{
    char buf[512];
    int r = getcwd(buf, sizeof(buf));
    if (r < 0)
    {
        println("pwd: failed to get current directory");
        exit(1);
    }
    println(buf);
    return 0;
}