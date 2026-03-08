/**
 * Licensed under MIT License - URIX project
 * cat.c - Concatenate and display files
 * Usage: cat [-n] <file> [file2 ...]
 */
#include <urix.h>

#define BUF_SIZE 512
#ifndef O_RDONLY
#define O_RDONLY 0
#endif

static void print_uint(unsigned long long n)
{
    if (n == 0)
    {
        print("0");
        return;
    }
    char buf[24];
    char out[24];
    int i = 0, j = 0;
    while (n)
    {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i--)
    {
        out[j++] = buf[i];
    }
    out[j] = '\0';
    print(out);
}

static int cat_file(const char *path, int number_lines, int *line_num, int *at_line_start)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        print("cat: ");
        print(path);
        println(": no such file or directory");
        return 1;
    }

    char buf[BUF_SIZE];
    int bytes;

    while ((bytes = read(fd, buf, sizeof(buf))) > 0)
    {
        if (!number_lines)
        {
            /* Write massive chunk safely via VFS */
            write(STDOUT_FILENO, buf, bytes);
        }
        else
        {
            int chunk_start = 0;
            for (int i = 0; i < bytes; i++)
            {
                if (*at_line_start)
                {
                    print_uint((*line_num)++);
                    print("\t");
                    *at_line_start = 0;
                }

                if (buf[i] == '\n')
                {
                    write(STDOUT_FILENO, &buf[chunk_start], i - chunk_start + 1);
                    *at_line_start = 1;
                    chunk_start = i + 1;
                }
            }

            /* Flush any remaining characters in the buffer that didn't end in a newline */
            if (chunk_start < bytes)
            {
                write(STDOUT_FILENO, &buf[chunk_start], bytes - chunk_start);
            }
        }
    }
    print("\n");
    close(fd);
    return 0;
}

int main(int argc, char **argv)
{
    int flag_n = 0;
    int file_count = 0;
    int ret = 0;

    /* Maintain state across multiple files */
    int global_line_num = 1;
    int global_at_line_start = 1;

    if (argc < 2)
    {
        println("Usage: cat [-n] <file> [file2 ...]");
        exit(1);
    }

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-' && argv[i][1] != '\0' && argv[i][2] == '\0')
        {
            if (argv[i][1] == 'n')
            {
                flag_n = 1;
            }
            else
            {
                print("cat: unknown flag: ");
                println(argv[i]);
                exit(1);
            }
        }
        else
        {
            file_count++;
            ret |= cat_file(argv[i], flag_n, &global_line_num, &global_at_line_start);
        }
    }

    if (file_count == 0)
    {
        println("cat: no files specified");
        exit(1);
    }

    return ret;
}