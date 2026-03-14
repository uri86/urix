/*
 * Licensed under MIT License - URIX project.
 * grep.c - Print lines matching a substring, highlighting matches in red.
 *
 * Usage:
 *   grep <pattern> [file ...]
 *   cat file | grep <pattern>
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "urix.h"
#include "../liburix/conf.h"

#define COLOR_RED 4
#define LINE_MAX 1024

/*
 * find_substr — return pointer to first occurrence of a substring
 */
static const char *find_substr(const char *haystack, const char *needle)
{
    size_t nlen = strlen(needle);
    if (nlen == 0)
        return haystack;
    for (; *haystack; haystack++)
        if (strncmp(haystack, needle, nlen) == 0)
            return haystack;
    return NULL;
}

static void print_highlighted(const char *line, const char *pattern, uint64_t fg, uint64_t bg)
{
    size_t plen = strlen(pattern);
    const char *pos = line;
    const char *match;

    while ((match = find_substr(pos, pattern)) != NULL)
    {
        /* text before the match */
        if (match > pos)
        {
            change_terminal_color(fg, bg);
            write(STDOUT_FILENO, pos, (size_t)(match - pos));
        }

        /* the match itself */
        change_terminal_color(COLOR_RED, bg);
        write(STDOUT_FILENO, match, plen);

        pos = match + plen;
    }

    change_terminal_color(fg, bg);
    if (*pos)
        write(STDOUT_FILENO, pos, strlen(pos));

    putchar('\n');
}

static void process_fd(int fd, const char *pattern, uint64_t fg, uint64_t bg)
{
    char line[LINE_MAX];
    int len = 0;
    char c;

    while (read(fd, &c, 1) == 1)
    {
        if (c == '\n' || len >= LINE_MAX - 1)
        {
            line[len] = '\0';
            if (find_substr(line, pattern))
                print_highlighted(line, pattern, fg, bg);
            len = 0;
        }
        else
        {
            line[len++] = c;
        }
    }

    /* flush any unterminated last line */
    if (len > 0)
    {
        line[len] = '\0';
        if (find_substr(line, pattern))
            print_highlighted(line, pattern, fg, bg);
    }
}

int main(int argc, char *argv[])
{
    conf_t cfg;
    uint64_t fg = 7;
    uint64_t bg = 0;

    if (conf_load(&cfg, "/etc/shell.conf") == CONF_OK)
    {
        char val[CONF_VAL_LEN];

        if (conf_get(&cfg, "fg", val, sizeof(val)) == CONF_OK)
            fg = (uint64_t)atol(val);

        if (conf_get(&cfg, "bg", val, sizeof(val)) == CONF_OK)
            bg = (uint64_t)atol(val);
    }

    if (argc < 2)
    {
        println("usage: grep <pattern> [file ...]");
        exit(1);
    }

    const char *pattern = argv[1];

    if (argc >= 3)
    {
        for (int i = 2; i < argc; i++)
        {
            int fd = open(argv[i], 0);
            if (fd < 0)
            {
                print("grep: cannot open: ");
                println(argv[i]);
                continue;
            }
            process_fd(fd, pattern, fg, bg);
            close(fd);
        }
    }
    else
    {
        process_fd(STDIN_FILENO, pattern, fg, bg);
    }
    change_terminal_color(fg, bg);
    exit(0);
    return 0;
}