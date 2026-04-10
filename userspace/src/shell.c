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
#include "../liburix/conf.h"
#include <string.h>
#include <auth.h>

#define MAX_ARGS 16
#define BUF_SIZE 256
#define PATH_SIZE 512

#define SHELL_CONF "/etc/shell.conf"
#define DEFAULT_FG 7
#define DEFAULT_BG 0
#define COLOR_RED 4
#define COLOR_CYAN 3
#define SESSION_FILE "/etc/session_user"

static pid_t shell_pid;
static uint64_t g_bg = DEFAULT_BG, g_fg = DEFAULT_FG;
static uint64_t g_pc = COLOR_RED; // Path color
static uint64_t g_rc = COLOR_RED; // Root color

static void print_cwd_prompt(void)
{
    char cwd[PATH_SIZE];
    int r = getcwd(cwd, sizeof(cwd));
    
    char user[64];
    strcpy(user, "user");
    
    // Load dynamic colors directly every prompt so config updates immediately
    conf_t cfg;
    char val[CONF_VAL_LEN];
    if (conf_load(&cfg, SHELL_CONF) == CONF_OK) {
        if (conf_get(&cfg, "fg", val, sizeof(val)) == CONF_OK) g_fg = (uint64_t)atoi(val);
        if (conf_get(&cfg, "bg", val, sizeof(val)) == CONF_OK) g_bg = (uint64_t)atoi(val);
        if (conf_get(&cfg, "pc", val, sizeof(val)) == CONF_OK) g_pc = (uint64_t)atoi(val);
        if (conf_get(&cfg, "rc", val, sizeof(val)) == CONF_OK) g_rc = (uint64_t)atoi(val);
    }
    
    int fd = open(SESSION_FILE, O_RDONLY);
    if (fd >= 0) {
        ssize_t len = read(fd, user, sizeof(user) - 1);
        close(fd);
        if (len > 0) {
            user[len] = '\0';
            for (int i = 0; i < len; i++) {
                if (user[i] == '\n' || user[i] == '\r') {
                    user[i] = '\0';
                    break;
                }
            }
        }
    }
    
    int is_root = (strcmp(user, "root") == 0 || strcmp(user, "admin") == 0);
    uint64_t user_color = is_root ? g_rc : COLOR_CYAN;

    change_terminal_color(user_color, g_bg);
    print(user);
    change_terminal_color(g_fg, g_bg);
    print("@urix:");
    change_terminal_color(g_pc, g_bg); // apply configurable path color
    if (r > 0)
        print(cwd);
    else
        print("/");
    change_terminal_color(g_fg, g_bg);
    
    if (is_root)
        print("# ");
    else
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

    if (conf_get(&cfg, "bg", val, sizeof(val)) == CONF_OK)
        *bg = (uint64_t)atoi(val);
        
    if (conf_get(&cfg, "pc", val, sizeof(val)) == CONF_OK)
        g_pc = (uint64_t)atoi(val);
        
    if (conf_get(&cfg, "rc", val, sizeof(val)) == CONF_OK)
        g_rc = (uint64_t)atoi(val);
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

static void run_parsed(char **argv, int argc, int fd_in, int fd_out)
{
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
                if (is_session_file(argv[i + 1])) {
                    println("Access denied: Session file cannot be manually edited.");
                    exit(1);
                }
                require_root_for_ps(argv[i + 1]);
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
                if (is_session_file(argv[i + 1])) {
                    println("Access denied: Session file cannot be manually edited.");
                    exit(1);
                }
                require_root_for_ps(argv[i + 1]);
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

    if (argc == 0 || argv[0] == NULL) exit(0);

    /* Process pid built-in */
    if (strcmp(argv[0], "pid") == 0)
    {
        if (fd_in >= 0) { dup2(fd_in, STDIN_FILENO); close(fd_in); }
        if (fd_out >= 0) { dup2(fd_out, STDOUT_FILENO); close(fd_out); }
        
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
    char *argv[MAX_ARGS];
    for (int i = 0; i < MAX_ARGS; i++)
        argv[i] = NULL;

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

int main(void)
{
    shell_pid = getpid();
    
    clear_screen();
    prntlg();
    
    load_colors(&g_fg, &g_bg);
    change_terminal_color(g_fg, g_bg);

    println("URIX Shell v0.2");
    println("Type 'help' for commands");

    char buf[BUF_SIZE];
    char buf_copy[BUF_SIZE];

    while (1)
    {
        print("\x1b[?25h");
        print_cwd_prompt();

        int len = readline(buf, sizeof(buf));
        if (len == 0)
            continue;
            
        strcpy(buf_copy, buf);

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

        char *argv[MAX_ARGS];
        for (int i = 0; i < MAX_ARGS; i++) argv[i] = NULL;
        int argc = tokenize(buf_copy, argv, MAX_ARGS);
        
        if (argc == 0) continue;
        
        // Parent-side builtins that must mutate the shell itself
        if (strcmp(argv[0], "exit") == 0)
        {
            println("Goodbye!");
            exit(0);
        }
        
        if (strcmp(argv[0], "cd") == 0)
        {
            const char *target = "/";
            if (argc > 1) {
                target = argv[1];
            }
            if (chdir(target) < 0)
            {
                print("cd: no such directory: ");
                println(target);
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