/**
 * Licensed under MIT License - URIX project
 * init.c - init process for userspace, loads the shell
 */

#include <urix.h>

int main(void)
{
    println("Started init process.");
    mkdir("etc", 0775);
    while (1)
    {
        int pid = fork();
        if (pid > 0)
        {
            wait(NULL);
            println("Restating the shell");
        }
        else
        {
            exec("/bin/shell", NULL);
            exit(1);
        }
    }
}