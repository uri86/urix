/**
 * Licensed under MIT License - URIX project
 * shell.c - Basic shell for userspace
 * Responsibilities:
 * - define simple in shell function
 * - allow to call other programs that sit in /bin folder
 */
#include "urix.h"

#define CURSOR_BLOCK 219
#define MAX_ARGS 16
#define BUF_SIZE 256
#define PATH_SIZE 512

static void print_cwd_prompt(void)
{
    char cwd[PATH_SIZE];
    int r = getcwd(cwd, sizeof(cwd));

    print("user@urix:");
    if (r > 0)
        print(cwd);
    else
        print("/");
    print("> ");
}

static int tokenize(char *buf, char **argv, int max_args)
{
    int argc = 0;
    int len = strlen(buf);
    int in_token = 0;

    for (int i = 0; i <= len && argc < max_args - 1; i++)
    {
        if (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\0')
        {
            if (in_token)
            {
                buf[i] = '\0';
                in_token = 0;
            }
        }
        else
        {
            if (!in_token)
            {
                argv[argc++] = &buf[i];
                in_token = 1;
            }
        }
    }
    argv[argc] = NULL;
    return argc;
}

static int readline(char *buf, int max_len)
{
    int i = 0;
    while (i < max_len - 1)
    {
        putchar(CURSOR_BLOCK);
        int c = getchar();
        print("\b \b");

        if (c == '\n' || c == '\r')
        {
            putchar('\n');
            break;
        }
        else if ((c == 127 || c == 8) && i > 0)
        {
            i--;
            print("\b \b");
        }
        else if (c >= 32 && c <= 126)
        {
            buf[i++] = c;
            putchar(c);
        }
    }
    buf[i] = '\0';
    return i;
}

int main(void)
{
    clear_screen();
    prntlg();
    change_terminal_color(7, 0);
    println("URIX Shell v0.2");
    println("Type 'help' for commands");

    char buf[BUF_SIZE];
    char path[128];
    char *argv[MAX_ARGS];

    while (1)
    {
        print_cwd_prompt();

        int len = readline(buf, sizeof(buf));
        if (len == 0)
            continue;

        if (strcmp(buf, "help") == 0)
        {
            println("Built-in commands:");
            println("  help     - this help");
            println("  cd <dir> - change directory");
            println("  exit     - exit shell");
            println("  clr      - clear screen");
            println("  pid      - show PID");
            println("  proct    - process table");
            println("  pmmstat  - physical memory stats");
            println("  kmlcstat - kernel allocator stats");
            println("");
            println("External commands (in /bin):");
            println("  ls [-la] [path]         - list directory");
            println("  cat [-n] <file> [...]   - print file(s)");
            println("  touch <file> [...]      - create empty file(s)");
            println("  mkdir [-p] <dir> [...]  - create directory");
            println("  rm [-rf] <path> [...]   - remove file/directory");
            println("  cp <src> <dst>          - copy file");
            println("  mv <src> <dst>          - move/rename file");
            println("  echo [-n] [args...]     - print arguments");
            println("  pwd                     - print working directory");
            continue;
        }

        if (strcmp(buf, "exit") == 0)
        {
            println("Goodbye!");
            exit(0);
        }

        if (strcmp(buf, "clr") == 0)
        {
            clear_screen();
            continue;
        }

        if (strcmp(buf, "pid") == 0)
        {
            pid_t pid = getpid();
            print("PID: ");
            /* simple itoa */
            char tmp[16];
            int i = 0;
            if (pid == 0)
            {
                tmp[i++] = '0';
            }
            else
            {
                int n = pid;
                while (n)
                {
                    tmp[i++] = '0' + (n % 10);
                    n /= 10;
                }
            }
            /* reverse */
            for (int a = 0, b = i - 1; a < b; a++, b--)
            {
                char t = tmp[a];
                tmp[a] = tmp[b];
                tmp[b] = t;
            }
            tmp[i] = '\0';
            println(tmp);
            continue;
        }

        if (strcmp(buf, "prntlg") == 0)
        {
            prntlg();
            change_terminal_color(7, 0);
            continue;
        }

        if (strcmp(buf, "proct") == 0)
        {
            proct();
            continue;
        }
        if (strcmp(buf, "pmmstat") == 0)
        {
            pmmstat();
            continue;
        }
        if (strcmp(buf, "kmlcstat") == 0)
        {
            kmlcstat();
            continue;
        }

        if (buf[0] == 'c' && buf[1] == 'd' &&
            (buf[2] == ' ' || buf[2] == '\t' || buf[2] == '\0'))
        {
            const char *target = "/";
            if (buf[2] != '\0')
            {
                int j = 3;
                while (buf[j] == ' ' || buf[j] == '\t')
                    j++;
                if (buf[j] != '\0')
                    target = &buf[j];
            }
            if (chdir(target) < 0)
            {
                print("cd: no such directory: ");
                println(target);
            }
            continue;
        }

        int argc = tokenize(buf, argv, MAX_ARGS);
        if (argc == 0)
            continue;

        if (fork() > 0)
        {
            wait(NULL);
        }
        else
        {
            if (argv[0][0] != '/')
            {
                strcpy(path, "/bin/");
                char *d = path + 5;
                const char *s = argv[0];
                while (*s)
                    *d++ = *s++;
                *d = '\0';
            }
            else
            {
                strcpy(path, argv[0]);
            }

            if (exec(path, argv) < 0)
            {
                print("Unknown command: ");
                println(argv[0]);
                exit(1);
            }
        }
    }

    return 0;
}