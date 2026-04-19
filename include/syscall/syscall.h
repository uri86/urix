/*
 * Licensed under MIT License - URIX project.
 * syscall.h - System calls header file.
 * Responsibilities:
 * - defines all the types of system calls available
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

/* syscall types */
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
#define SYS_MAX 36

/* file descriptor constants */
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* open flags */
#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR 0x0002
#define O_CREAT 0x0040
#define O_EXCL 0x0080
#define O_TRUNC 0x0200
#define O_APPEND 0x0400

/* seek whence values */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* error codes */
#define EPERM 1
#define ENOENT 2
#define ESRCH 3
#define EINTR 4
#define EIO 5
#define ENXIO 6
#define ENOEXEC 8
#define EBADF 9
#define ECHILD 10
#define EAGAIN 11
#define ENOMEM 12
#define EACCES 13
#define EFAULT 14
#define EBUSY 16
#define EEXIST 17
#define ENOTDIR 20
#define EISDIR 21
#define EINVAL 22
#define ENFILE 23
#define EMFILE 24
#define ENOSPC 28
#define EROFS 30
#define ENOSYS 38

/* info screen print types */
#define KPPMM 1
#define KPMAL 2
#define KPLG 3
#define KPPPT 4
#define KPPCB 5
#define KPVFST 6
#define KPFSSTAT 7
#define KPDIR 8
#define KPFILEBLK 9

/**
 * syscall_init - initializes the system calls handler
 */
void syscall_init(void);

/**
 * syscall_handler - main system call handler
 */
long syscall_handler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6);
#endif /* SYSCALL_H */