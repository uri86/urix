/*
 * Licensed under MIT License - URIX project.
 * fd_table.h - Per-process file descriptor table
 */

#ifndef FD_TABLE_H
#define FD_TABLE_H

#include <stdint.h>
#include <stddef.h>
#include <fs/vfs.h>

#define MAX_FDS 64

/* file descriptor types */
typedef enum {
    FD_TYPE_NONE = 0,
    FD_TYPE_FILE,
    FD_TYPE_CONSOLE,
    FD_TYPE_PIPE
} fd_type_t;

/* file descriptor entry */
typedef struct fd_entry {
    fd_type_t type;
    int flags;
    
    union {
        file_t *vfs_file;
        void *pipe;
        int console_type;
    };
} fd_entry_t;

typedef struct {
    fd_entry_t fds[MAX_FDS];
    int next_fd;
} fd_table_t;

/**
 * fd_table_create - Create a new file descriptor table
 * 
 * Sets up stdin (0), stdout (1), stderr (2) as console
 * 
 * Returns: Pointer to new table, or NULL on failure
 */
fd_table_t *fd_table_create(void);

/**
 * fd_table_destroy - Free a file descriptor table
 * 
 * Closes all open file descriptors and frees the table
 */
void fd_table_destroy(fd_table_t *table);

/**
 * fd_table_alloc - Allocate a new file descriptor
 * 
 * Returns: File descriptor number, or -1 if table is full
 */
int fd_table_alloc(fd_table_t *table);

/**
 * fd_table_get - Get file descriptor entry
 * 
 * Returns: Pointer to entry, or NULL if fd is invalid
 */
fd_entry_t *fd_table_get(fd_table_t *table, int fd);

/**
 * fd_table_free - Free a file descriptor
 */
void fd_table_free(fd_table_t *table, int fd);

/**
 * fd_table_dup - Duplicate a file descriptor
 * 
 * Returns: New fd number, or -1 on failure
 */
int fd_table_dup(fd_table_t *table, int oldfd);

/**
 * fd_table_dup2 - Duplicate fd to specific number
 * 
 * Returns: newfd on success, or -1 on failure
 */
int fd_table_dup2(fd_table_t *table, int oldfd, int newfd);

#endif /* FD_TABLE_H */