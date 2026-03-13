/**
 * Licensed under MIT License - URIX project
 * syscall.h - User space syscall header file
 * Responsibilities:
 * - exposes syscall function for use in user space.
 * - define all syscall numbers
 * Notes:
 * - syscall numbers must match the numbers in syscall.h at urix/include/syscall/syscall.h
 */

#ifndef SYSCALL_H
#define SYSCALL_H

/* system call numbers */
#define SYS_EXIT 0
#define SYS_READ 1
#define SYS_WRITE 2
#define SYS_OPEN 3
#define SYS_CLOSE 4
#define SYS_FORK 5
#define SYS_EXEC 6
#define SYS_WAIT 7
#define SYS_GETPID 8
#define SYS_SLEEP 9
#define SYS_YIELD 10
#define SYS_KILL 11
#define SYS_BRK 12
#define SYS_SBRK 13
#define SYS_MMAP 14
#define SYS_MUNMAP 15
#define SYS_CHDIR 16
#define SYS_GETCWD 17
#define SYS_MKDIR 18
#define SYS_RMDIR 19
#define SYS_UNLINK 20
#define SYS_STAT 21
#define SYS_FSTAT 22
#define SYS_SEEK 23
#define SYS_DUP 24
#define SYS_DUP2 25
#define SYS_PIPE 26
#define SYS_GETCHAR 27
#define SYS_PUTCHAR 28
#define SYS_GETS 29
#define SYS_PUTS 30
#define SYS_KPS 31
#define SYS_TERMINAL_COLOR 32
#define SYS_CLEAR_SCREEN 33
#define SYS_READDIR 34
#define SYS_ISATTY 35

/* info screen print types */
#define KPPMM 1
#define KPMAL 2
#define KPLG 3
#define KPPPT 4

/* open flags */
#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR 0x0002
#define O_CREAT 0x0040
#define O_EXCL 0x0080
#define O_TRUNC 0x0200
#define O_APPEND 0x0400

static inline long syscall0(long n)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(n) : "memory");
    return ret;
}

static inline long syscall1(long n, long a1)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(n), "D"(a1) : "memory");
    return ret;
}

static inline long syscall2(long n, long a1, long a2)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2) : "memory");
    return ret;
}

static inline long syscall3(long n, long a1, long a2, long a3)
{
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "memory");
    return ret;
}

#endif /* SYSCALL_H */