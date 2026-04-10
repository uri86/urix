/**
 * Licensed under MIT License - URIX project.
 * rm.c - Remove files and directories
 * Usage: rm [-r] [-f] <path> [path2 ...]
 */
#include <urix.h>
#include <string.h>
#include <auth.h>

static int flag_r = 0;
static int flag_f = 0;

static int do_rm(const char *path);

static int rm_dir(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        if (!flag_f)
        {
            print("rm: cannot open directory '");
            print(path);
            println("'");
        }
        return 1;
    }

    dirent_t entry;
    /* Build child path buffer */
    char child[512];

    while (readdir(fd, &entry) == 0)
    {
        /* skip . and .. */
        if (entry.name[0] == '.' &&
            (entry.name[1] == '\0' ||
             (entry.name[1] == '.' && entry.name[2] == '\0')))
            continue;

        /* build full child path */
        int pi = 0;
        for (int i = 0; path[i] && pi < 500; i++)
            child[pi++] = path[i];
        if (pi > 0 && child[pi - 1] != '/')
            child[pi++] = '/';
        for (int i = 0; entry.name[i] && pi < 510; i++)
            child[pi++] = entry.name[i];
        child[pi] = '\0';

        do_rm(child);
    }
    close(fd);

    int r = rmdir(path);
    if (r < 0 && !flag_f)
    {
        print("rm: cannot remove directory '");
        print(path);
        println("'");
        return 1;
    }
    return 0;
}

static int do_rm(const char *path)
{
    if (strcmp(path, "/") == 0 && flag_r) {
        if (!is_locked_command_allowed()) {
            println("rm: it is dangerous to operate recursively on '/' without root privileges.");
            return 1;
        }
    }

    /* try unlink */
    int r = unlink(path);
    if (r == 0)
        return 0;

    /* try a directory */
    if (flag_r)
    {
        return rm_dir(path);
    }

    if (!flag_f)
    {
        print("rm: cannot remove '");
        print(path);
        println("': is a directory (use -r)");
    }
    return 1;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        println("Usage: rm [-r] [-f] <path> [path2 ...]");
        exit(1);
    }

    int path_count = 0;
    int ret = 0;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            for (int j = 1; argv[i][j]; j++)
            {
                if (argv[i][j] == 'r' || argv[i][j] == 'R')
                    flag_r = 1;
                else if (argv[i][j] == 'f')
                    flag_f = 1;
                else
                {
                    print("rm: unknown flag: -");
                    putchar(argv[i][j]);
                    print("\n");
                    exit(1);
                }
            }
        }
        else
        {
            path_count++;
            ret |= do_rm(argv[i]);
        }
    }

    if (path_count == 0)
    {
        println("rm: missing operand");
        exit(1);
    }

    return ret;
}