/**
 * Licensed under MIT License - URIX project
 * uptimedemo.c - Multitasking and uptime demonstration
 */
#include <urix.h>
#include <stdio.h>

void delay(int count)
{
    for (volatile int i = 0; i < count; i++)
    {
        for (volatile int j = 0; j < 100000; j++)
        {
            // Just burn some CPU cycles
        }
    }
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    char buf[128];

    print("Starting uptimedemo...\n");

    pid_t cruncher = fork();
    if (cruncher == 0)
    {
        // Child 1: Number crunching
        for (int i = 0; i < 20; i++)
        {
            snprintf(buf, sizeof(buf), "[Cruncher] Crunching numbers... (iteration %d)\n", i + 1);
            print(buf);
            delay(500);
        }
        print("[Cruncher] Done!\n");
        exit(0);
    }

    pid_t up_printer = fork();
    if (up_printer == 0)
    {
        // Child 2: Prints uptime periodically
        for (int i = 0; i < 20; i++)
        {
            long secs = uptime();
            snprintf(buf, sizeof(buf), "[UptimePrinter] System has been up for %ld seconds.\n", secs);
            print(buf);
            delay(500);
        }
        print("[UptimePrinter] Done!\n");
        exit(0);
    }

    // Parent waits for both
    int status1, status2;
    wait(&status1);
    wait(&status2);

    print("uptimedemo finished. Both children exited.\n");
    return 0;
}
