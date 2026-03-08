/**
 * Licensed under MIT License - URIX project.
 * touch.c - Create empty files
 */
#include <urix.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        println("Usage: touch <file> [file2 ...]");
        exit(1);
    }

    int ret = 0;

    for (int i = 1; i < argc; i++) {
        /* O_CREAT | O_WRONLY — create if missing, open for writing */
        int fd = open(argv[i], O_CREAT | O_WRONLY);
        if (fd < 0) {
            print("touch: cannot create '");
            print(argv[i]);
            println("'");
            ret = 1;
            continue;
        }
        close(fd);
    }

    return ret;
}