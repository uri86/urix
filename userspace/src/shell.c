/**
 * Licensed under MIT License - URIX project
 * shell.c - Basic shell for userspace
 * Responsibilities:
 * - define simple in shell function
 * - allow to call other programs that sit in /bin folder
 * - handle redirects in the user input and pipes
 */
#include "urix.h"
#include "../liburix/conf.h"
#include "string.h"

#define MAX_ARGS 16
#define BUF_SIZE 256
#define PATH_SIZE 512

#define SHELL_CONF "/etc/shell.conf"
#define DEFAULT_FG 7
#define DEFAULT_BG 0

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
        int c = getchar();

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

/*
 * load_colors - read fg and bg from /etc/shell.conf.
 * Falls back to defaults if the file is missing or a key is absent.
 */
static void load_colors(uint64_t *fg, uint64_t *bg)
{
    conf_t cfg;
    char val[CONF_VAL_LEN];

    *fg = DEFAULT_FG;
    *bg = DEFAULT_BG;

    if (conf_load(&cfg, SHELL_CONF) != CONF_OK)
        return;

    if (conf_get(&cfg, "fg", val, sizeof(val)) == CONF_OK)
        *fg = (uint64_t)atoi(val);

    if (conf_get(&cfg, "bg", val, sizeof(val)) == CONF_OK)
        *bg = (uint64_t)atoi(val);
}

/*
 * do_exec - build a /bin/<cmd> path and exec with given argv[0..argc-1].
 *
 * Called from the child process only. Never returns on success.
 * On failure prints an error and calls exit(1).
 */
static void do_exec(char **argv, int argc, int fd_in, int fd_out)
{
    char path[PATH_SIZE];

    if (argc == 0 || argv[0] == NULL)
        exit(0);

    /* Wire up pipe ends if requested */
    if (fd_in >= 0)
    {
        dup2(fd_in, STDIN_FILENO);
        close(fd_in);
    }
    if (fd_out >= 0)
    {
        dup2(fd_out, STDOUT_FILENO);
        close(fd_out);
    }

    if (argv[0][0] == '/' || (argv[0][0] == '.' && argv[0][1] == '/'))
    {
        char *d = path;
        const char *s = argv[0];
        while (*s && d < path + PATH_SIZE - 1)
            *d++ = *s++;
        *d = '\0';
    }
    else
    {
        path[0] = '/';
        path[1] = 'b';
        path[2] = 'i';
        path[3] = 'n';
        path[4] = '/';
        path[5] = '\0';
        char *d = path + 5;
        const char *s = argv[0];
        while (*s && d < path + PATH_SIZE - 1)
            *d++ = *s++;
        *d = '\0';
    }

    if (exec(path, argv) < 0)
    {
        print("Unknown command: ");
        print(argv[0]);
        print("\n");
        exit(1);
    }
    exit(1);
}

/*
 * run_cmd - tokenize cmd_buf, handle > / >> redirects, then exec.
 * Must be called from a child process.
 */
static void run_cmd(char *cmd_buf, int fd_in, int fd_out)
{
    char *argv[MAX_ARGS];
    int argc;

    for (int i = 0; i < MAX_ARGS; i++)
        argv[i] = NULL;

    argc = tokenize(cmd_buf, argv, MAX_ARGS);
    if (argc == 0)
        exit(0);

    for (int i = 0; i < argc; i++)
    {
        if (argv[i] == NULL)
            break;

        if (strcmp(argv[i], ">") == 0)
        {
            if (argv[i + 1] != NULL)
            {
                int rfd = open(argv[i + 1], O_WRONLY | O_CREAT | O_TRUNC);
                if (rfd >= 0)
                {
                    dup2(rfd, STDOUT_FILENO);
                    close(rfd);
                }
            }
            argv[i] = NULL;
            argc = i;
            break;
        }
        else if (strcmp(argv[i], ">>") == 0)
        {
            if (argv[i + 1] != NULL)
            {
                int rfd = open(argv[i + 1], O_WRONLY | O_CREAT | O_APPEND);
                if (rfd >= 0)
                {
                    dup2(rfd, STDOUT_FILENO);
                    close(rfd);
                }
            }
            argv[i] = NULL;
            argc = i;
            break;
        }
    }

    do_exec(argv, argc, fd_in, fd_out);
}

/*
 * run_piped - run two commands connected by a pipe.
 */
static void run_piped(char *left_buf, char *right_buf)
{
    int pipefd[2];
    if (pipe(pipefd) < 0)
    {
        println("pipe: failed to create pipe");
        return;
    }

    // stdout redirected to pipe end (left command)
    if (fork() == 0)
    {
        close(pipefd[0]);
        run_cmd(left_buf, -1, pipefd[1]);
    }

    // stdin redirected into read end (right command)
    if (fork() == 0)
    {
        close(pipefd[1]);
        run_cmd(right_buf, pipefd[0], -1);
    }

    close(pipefd[0]);
    close(pipefd[1]);
    wait(NULL);
    wait(NULL);
}

int main(void)
{
    clear_screen();
    prntlg();

    uint64_t fg, bg;
    load_colors(&fg, &bg);
    change_terminal_color(fg, bg);

    println("URIX Shell v0.2");
    println("Type 'help' for commands");

    char buf[BUF_SIZE];

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
            println("");
            println("Redirects:  cmd > file   cmd >> file   cmd1 | cmd2");
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
            change_terminal_color(fg, bg);
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

        char *pipe_pos = NULL;
        for (int i = 0; buf[i] != '\0'; i++)
        {
            if (buf[i] == '|')
            {
                pipe_pos = &buf[i];
                break;
            }
        }

        if (pipe_pos != NULL)
        {
            *pipe_pos = '\0';
            char *left_cmd = buf;
            char *right_cmd = pipe_pos + 1;
            while (*right_cmd == ' ' || *right_cmd == '\t')
                right_cmd++;
            run_piped(left_cmd, right_cmd);
            continue;
        }

        if (fork() == 0)
        {
            run_cmd(buf, -1, -1);
        }
        wait(NULL);
    }

    return 0;
}