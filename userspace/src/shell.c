#include "urix.h"

int main(void)
{
    println("URIX Shell v0.1");
    println("Type 'help' for commands");

    while (1)
    {
        print("user@urix> ");

        char buf[128];
        memset(buf, 0, sizeof(buf));

        int i = 0;
        while (i < 127)
        {
            int c = getchar();
            if (c == '\n' || c == '\r')
            {
                putchar('\n');
                break;
            }
            if (c == 127 || c == 8)
            {
                if (i > 0)
                {
                    i--;
                    print("\b");
                }
                continue;
            }
            if (c >= 32 && c < 127)
            {
                buf[i++] = c;
                putchar(c);
            }
        }
        buf[i] = '\0';

        if (strcmp(buf, "help") == 0)
        {
            println("Available commands:");
            println("  help  - Show this help");
            println("  hello - Print greeting");
            println("  pid   - Show process ID");
            println("  yield - Yield CPU");
            println("  exit  - Exit shell");
        }
        else if (strcmp(buf, "hello") == 0)
        {
            int pid = fork();
            int status;
            if (pid > 0)
            {
                wait(&status);
            }
            else
            {
                exec("/bin/hello", NULL);
            }

        }
        else if (strcmp(buf, "pid") == 0)
        {
            pid_t pid = getpid();
            print("PID: ");
            if (pid >= 100)
                putchar('0' + (pid / 100));
            if (pid >= 10)
                putchar('0' + ((pid / 10) % 10));
            putchar('0' + (pid % 10));
            putchar('\n');
        }
        else if (strcmp(buf, "exit") == 0)
        {
            println("Goodbye!");
            exit(0);
        }
        else if(strcmp(buf, "prntlg") == 0)
        {
            prntlg();
        }
        else if (strcmp(buf, "proct") == 0)
        {
            proct();
        }
        else if(strcmp(buf, "pmmstat") == 0)
        {
            pmmstat();
        }
        else if(strcmp(buf, "kmlcstat") == 0)
        {
            kmlcstat();
        }
        else if (buf[0] != '\0')
        {
            print("Unknown command: ");
            println(buf);
        }
    }

    return 0;
}
