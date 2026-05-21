/**
 * Licensed under MIT License - URIX project
 * shell.c - Basic shell for userspace
 * Responsibilities:
 * - define simple in shell function
 * - allow to call other programs that sit in /bin folder
 * - handle redirects in the user input and pipes
 */
#include "urix.h"
#include <stdio.h>
#include <string.h>
#include <auth.h>
#include "../liburix/conf.h"

#define MAX_ARGS 16
#define BUF_SIZE 256
#define PATH_SIZE 512
#define HISTORY_MAX 20

#define SHELL_CONF "/etc/shell.conf"
#define DEFAULT_FG 7
#define DEFAULT_BG 0
#define COLOR_RED 4
#define COLOR_CYAN 3
#define SESSION_FILE "/etc/session_user"

static pid_t shell_pid;
static uint64_t g_bg = DEFAULT_BG;
static uint64_t g_fg = DEFAULT_FG;
static uint64_t g_pc = COLOR_RED; // Path color
static uint64_t g_rc = COLOR_RED; // Root color

static char history[HISTORY_MAX][BUF_SIZE];
static int history_count = 0;

static void print_cwd_prompt(void)
{
    char cwd[PATH_SIZE];
    char user[64] = "user";
    char val[CONF_VAL_LEN];

    int r = getcwd(cwd, sizeof(cwd));
    conf_t cfg;

    if (conf_load(&cfg, SHELL_CONF) == CONF_OK)
    {
        if (conf_get(&cfg, "fg", val, sizeof(val)) == CONF_OK)
            g_fg = atoi(val);

        if (conf_get(&cfg, "bg", val, sizeof(val)) == CONF_OK)
            g_bg = atoi(val);

        if (conf_get(&cfg, "pc", val, sizeof(val)) == CONF_OK)
            g_pc = atoi(val);

        if (conf_get(&cfg, "rc", val, sizeof(val)) == CONF_OK)
            g_rc = atoi(val);
    }

    int fd = open(SESSION_FILE, O_RDONLY);
    if (fd >= 0)
    {
        ssize_t len = read(fd, user, sizeof(user) - 1);
        close(fd);
        if (len > 0)
        {
            user[len] = '\0';
            char *p = strchr(user, '\n');
            if (p)
                *p = '\0';

            p = strchr(user, '\r');
            if (p)
                *p = '\0';
        }
    }

    int is_root = (!strcmp(user, "root") || !strcmp(user, "admin"));

    change_terminal_color(is_root ? g_rc : COLOR_CYAN, g_bg);
    print(user);

    change_terminal_color(g_fg, g_bg);
    print("@urix:");

    change_terminal_color(g_pc, g_bg);
    print(r > 0 ? cwd : "/");
    change_terminal_color(g_fg, g_bg);
    print(is_root ? "# " : "> ");
}

static int tokenize(char *buf, char **argv, int max_args)
{
    int argc = 0;
    while (*buf && argc < max_args - 1)
    {
        while (*buf == ' ' || *buf == '\t')
            *buf++ = '\0';

        if (!*buf)
            break;

        argv[argc++] = buf;

        while (*buf && *buf != ' ' && *buf != '\t')
            buf++;
    }
    argv[argc] = NULL;
    return argc;
}

static int readline(char *buf, int max_len)
{
    int i = 0;
    int history_idx = history_count;

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
        else if (c == '\x1b')
        {
            if (getchar() == '[')
            {
                int c3 = getchar();
                if (c3 == 'A' && history_idx > 0)
                {
                    history_idx--;
                }
                else if (c3 == 'B' && history_idx < history_count)
                {
                    history_idx++;
                }
                else
                {
                    continue;
                }

                for (int j = 0; j < i; j++)
                    print("\b \b");

                if (history_idx < history_count)
                {
                    strcpy(buf, history[history_idx]);
                    i = strlen(buf);
                }
                else
                {
                    buf[0] = '\0';
                    i = 0;
                }
                print(buf);
            }
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
 * load_colors - read fg, bg from /etc/shell.conf for base state.
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
    else
        conf_set(&cfg, "fg", "7");

    if (conf_get(&cfg, "bg", val, sizeof(val)) == CONF_OK)
        *bg = (uint64_t)atoi(val);
    else
        conf_set(&cfg, "bg", "0");

    if (conf_get(&cfg, "pc", val, sizeof(val)) == CONF_OK)
        g_pc = (uint64_t)atoi(val);
    else
        conf_set(&cfg, "pc", "4");

    if (conf_get(&cfg, "rc", val, sizeof(val)) == CONF_OK)
        g_rc = (uint64_t)atoi(val);
    else
        conf_set(&cfg, "rc", "4");

    conf_save(&cfg);
}

/*
 * do_exec - build a /bin/<cmd> path and exec with given argv[0..argc-1].
 *
 * Called from the child process only. Never returns on success.
 * On failure prints an error and calls exit(1).
 */
static void do_exec(char **argv, int argc, int fd_in, int fd_out)
{
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

    char path[PATH_SIZE];

    if (argv[0][0] == '/' || (argv[0][0] == '.' && argv[0][1] == '/'))
    {
        strcpy(path, argv[0]);
    }
    else
    {
        if (strlen(argv[0]) >= PATH_SIZE - 6)
            exit(1);

        strcpy(path, "/bin/");
        strcat(path, argv[0]);
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

static void run_parsed(char **argv, int argc, int fd_in, int fd_out)
{
    if (argc == 0)
        exit(0);

    for (int i = 0; i < argc && argv[i]; i++)
    {
        int is_out = (strcmp(argv[i], ">") == 0);
        int is_app = (strcmp(argv[i], ">>") == 0);

        if (is_out || is_app)
        {
            if (argv[i + 1] != NULL)
            {
                if (is_session_file(argv[i + 1]))
                {
                    println("Access denied: Session file cannot be manually edited.");
                    exit(1);
                }

                require_root_for_ps(argv[i + 1]);
                int fd = open(argv[i + 1], O_WRONLY | O_CREAT | (is_app ? O_APPEND : O_TRUNC));

                if (fd >= 0)
                {
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
            }
            argv[i] = NULL;
            argc = i;
            break;
        }
    }

    if (argc == 0 || argv[0] == NULL)
        exit(0);

    if (strcmp(argv[0], "pid") == 0)
    {
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

        char out[32];
        snprintf(out, sizeof(out), "PID: %d", shell_pid);
        println(out);
        exit(0);
    }
    else if (strcmp(argv[0], "cd") == 0 || strcmp(argv[0], "exit") == 0)
    {
        exit(0);
    }

    do_exec(argv, argc, fd_in, fd_out);
}

/*
 * run_cmd - process argv, handle > / >> redirects, then exec.
 * Must be called from a child process.
 */
static void run_cmd(char *cmd_buf, int fd_in, int fd_out)
{
    char *argv[MAX_ARGS] = {NULL};
    int argc = tokenize(cmd_buf, argv, MAX_ARGS);
    run_parsed(argv, argc, fd_in, fd_out);
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

/*
 * run_ampersand - run two commands, left one silenced and lives until right one finishes.
 */
static void run_ampersand(char *left_buf, char *right_buf)
{
    int pipefd[2];
    if (pipe(pipefd) < 0)
    {
        println("pipe: failed to create pipe for ampersand");
        return;
    }

    pid_t left_pid = fork();
    if (left_pid == 0)
    {
        close(pipefd[0]); // We don't read from the pipe
        run_cmd(left_buf, -1, pipefd[1]); // Redirect output to pipe
        exit(0);
    }

    pid_t right_pid = fork();
    if (right_pid == 0)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        run_cmd(right_buf, -1, -1);
        exit(0);
    }

    // Shell closes both ends
    close(pipefd[0]);
    close(pipefd[1]);

    int left_alive = 1;
    int right_alive = 1;

    while (right_alive)
    {
        int status;
        pid_t p = wait(&status);
        if (p == right_pid)
        {
            right_alive = 0;
        }
        else if (p == left_pid)
        {
            left_alive = 0;
        }
        else if (p < 0)
        {
            break;
        }
    }

    if (left_alive)
    {
        kill(left_pid, 9);
        while (1)
        {
            int status;
            pid_t p = wait(&status);
            if (p == left_pid || p < 0)
            {
                break;
            }
        }
    }
}

int main(void)
{
    shell_pid = getpid();
    clear_screen();
    prntlg();

    load_colors(&g_fg, &g_bg);
    change_terminal_color(g_fg, g_bg);

    println("URIX Shell v0.3");
    println("Type 'help' for commands");

    char buf[BUF_SIZE];
    char buf_copy[BUF_SIZE];

    while (1)
    {
        print("\x1b[?25h");
        print_cwd_prompt();

        if (readline(buf, sizeof(buf)) == 0)
            continue;

        strcpy(buf_copy, buf);

        if (history_count == 0 || strcmp(history[history_count - 1], buf_copy) != 0)
        {
            if (history_count < HISTORY_MAX)
            {
                strcpy(history[history_count++], buf_copy);
            }
            else
            {
                for (int j = 1; j < HISTORY_MAX; j++)
                {
                    strcpy(history[j - 1], history[j]);
                }
                strcpy(history[HISTORY_MAX - 1], buf_copy);
            }
        }

        char *pipe_pos = strchr(buf, '|');

        if (pipe_pos)
        {
            *pipe_pos++ = '\0';
            while (*pipe_pos == ' ' || *pipe_pos == '\t')
                pipe_pos++;

            run_piped(buf, pipe_pos);
            continue;
        }

        char *amp_pos = strchr(buf, '&');

        if (amp_pos)
        {
            *amp_pos++ = '\0';
            while (*amp_pos == ' ' || *amp_pos == '\t')
                amp_pos++;

            run_ampersand(buf, amp_pos);
            continue;
        }

        char *argv[MAX_ARGS] = {NULL};
        int argc = tokenize(buf_copy, argv, MAX_ARGS);

        if (argc == 0)
            continue;

        if (strcmp(argv[0], "exit") == 0)
        {
            println("Goodbye!");
            exit(0);
        }

        if (strcmp(argv[0], "cd") == 0)
        {
            const char *dst = "/";
            if (argc > 1)
            {
                dst = argv[1];
            }
            if (chdir(dst) < 0)
            {
                print("cd: no such directory: ");
                println(dst);
            }
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