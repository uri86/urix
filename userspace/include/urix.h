/**
 * Licensed under MIT License - URIX project
 * urix.h - holds the function definitions for interrupts and kernel services
 * Responsibilities:
 * - define the basic types needed in the userspace
 * - expose some simple lib c style function written in liburix
 * - define and expose user ready interrupt functions, such as print or println
 */

#ifndef URIX_H
#define URIX_H

#include "syscall.h"

typedef long ssize_t;
typedef unsigned long size_t;
typedef int pid_t;
typedef unsigned long long uint64_t;
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define VFS_MAX_NAME 256

typedef struct
{
    uint64_t inode;
    char name[VFS_MAX_NAME];
    uint8_t type; /* 1 = file, 2 = dir */
} dirent_t;

size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);

static inline void exit(int status)
{
    syscall1(SYS_EXIT, status);
    while (1)
    {
    }
}

static inline ssize_t read(int fd, void *buf, size_t count)
{
    return syscall3(SYS_READ, fd, (long)buf, count);
}

static inline ssize_t write(int fd, const void *buf, size_t count)
{
    return syscall3(SYS_WRITE, fd, (long)buf, count);
}

static inline int open(const char *path, int flags)
{
    return syscall2(SYS_OPEN, (long)path, flags);
}

static inline int close(int fd)
{
    return syscall1(SYS_CLOSE, fd);
}

static inline pid_t fork(void)
{
    return syscall0(SYS_FORK);
}

static inline int exec(const char *path, char *const argv[])
{
    return syscall2(SYS_EXEC, (long)path, (long)argv);
}

static inline pid_t wait(int *status)
{
    return syscall1(SYS_WAIT, (long)status);
}

static inline pid_t getpid(void)
{
    return syscall0(SYS_GETPID);
}

static inline void yield(void)
{
    syscall0(SYS_YIELD);
}

static inline int kill(pid_t pid, int sig)
{
    return syscall2(SYS_KILL, pid, sig);
}

static inline int mkdir(const char *path, int mode)
{
    return syscall2(SYS_MKDIR, (long)path, mode);
}

static inline int rmdir(const char *path)
{
    return syscall1(SYS_RMDIR, (long)path);
}

static inline int unlink(const char *path)
{
    return syscall1(SYS_UNLINK, (long)path);
}

static inline int getchar(void)
{
    return syscall0(SYS_GETCHAR);
}

static inline int putchar(int c)
{
    return syscall1(SYS_PUTCHAR, c);
}

static inline int puts(const char *str)
{
    return syscall1(SYS_PUTS, (long)str);
}

static inline char *gets(char *buf, size_t size)
{
    syscall2(SYS_GETS, (long)buf, size);
    return buf;
}

static inline int readdir(int fd, dirent_t *entry)
{
    return syscall2(SYS_READDIR, fd, (long)entry);
}

static inline int getcwd(char *buf, size_t size)
{
    return syscall2(SYS_GETCWD, (long)buf, size);
}

static inline int chdir(const char *path)
{
    return syscall1(SYS_CHDIR, (long)path);
}

static inline void print(const char *s)
{
    write(STDOUT_FILENO, s, strlen(s));
}

static inline void println(const char *s)
{
    print(s);
    putchar('\n');
}

static inline void proct(void) { syscall1(SYS_KPS, KPPPT); }
static inline void pmmstat(void) { syscall1(SYS_KPS, KPPMM); }
static inline void prntlg(void) { syscall1(SYS_KPS, KPLG); }
static inline void kmlcstat(void) { syscall1(SYS_KPS, KPMAL); }

static inline long change_terminal_color(uint64_t fg, uint64_t bg)
{
    return syscall2(SYS_TERMINAL_COLOR, fg, bg);
}

static inline void clear_screen(void)
{
    syscall0(SYS_CLEAR_SCREEN);
}

static inline int dup2(int oldfd, int newfd)
{
    return syscall2(SYS_DUP2, oldfd, newfd);
}

static inline int pipe(int pipefd[2])
{
    return (int)syscall1(SYS_PIPE, (long)pipefd);
}

static inline int isatty(int fd)
{
    return (int)syscall1(SYS_ISATTY, fd);
}

#endif /* URIX_H */