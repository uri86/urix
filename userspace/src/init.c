/**
 * Licensed under MIT License - URIX project
 * init.c - init process for userspace, loads the shell
 */

#include <urix.h>

int main(void)
{
    println("Started init process.");
    pid_t pid = getpid();
    print("PID: ");
    if (pid >= 100)
        putchar('0' + (pid / 100));
    if (pid >= 10)
        putchar('0' + ((pid / 10) % 10));
    putchar('0' + (pid % 10));
    putchar('\n');
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