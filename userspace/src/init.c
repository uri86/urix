/**
 * Licensed under MIT License - URIX project
 * init.c - init process for userspace, loads the login program
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
            int status;
            wait(&status);
            println("Restarting the login process");
            yield();
        }
        else
        {
            exec("/bin/login", NULL);
            exit(1);
        }
    }
}