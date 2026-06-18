/*
 * Licensed under MIT License - URIX project.
 * fd_table.c - File descriptor table implementation
 */

#include <process/fd_table.h>
#include <memory/kmalloc.h>
#include <string.h>
#include <syscall/syscall.h>
#include <process/pipe.h>

fd_table_t *fd_table_create(void)
{
    fd_table_t *table = kmalloc(sizeof(fd_table_t));
    if (!table)
        return NULL;

    memset(table, 0, sizeof(fd_table_t));

    // standard IO
    table->fds[0].type = FD_TYPE_CONSOLE;
    table->fds[0].flags = O_RDONLY;
    table->fds[0].console_type = 0;

    table->fds[1].type = FD_TYPE_CONSOLE;
    table->fds[1].flags = O_WRONLY;
    table->fds[1].console_type = 1;

    table->fds[2].type = FD_TYPE_CONSOLE;
    table->fds[2].flags = O_WRONLY;
    table->fds[2].console_type = 2;

    table->next_fd = 3;

    return table;
}

void fd_table_destroy(fd_table_t *table)
{
    if (!table)
        return;

    for (int i = 0; i < MAX_FDS; i++)
    {
        if (table->fds[i].type != FD_TYPE_NONE)
        {
            fd_table_free(table, i);
        }
    }

    kfree(table);
}

int fd_table_alloc(fd_table_t *table)
{
    if (!table)
        return -1;

    for (int i = table->next_fd; i < MAX_FDS; i++)
    {
        if (table->fds[i].type == FD_TYPE_NONE)
        {
            table->next_fd = i + 1;
            return i;
        }
    }

    // if the first loop fails, tries again from the start
    for (int i = 0; i < table->next_fd; i++)
    {
        if (table->fds[i].type == FD_TYPE_NONE)
        {
            return i;
        }
    }

    return -1; // returns on failure
}

fd_entry_t *fd_table_get(fd_table_t *table, int fd)
{
    if (!table || fd < 0 || fd >= MAX_FDS)
        return NULL;

    if (table->fds[fd].type == FD_TYPE_NONE)
        return NULL;

    return &table->fds[fd]; // returns a pointer to the fd entry
}

void fd_table_free(fd_table_t *table, int fd)
{
    if (!table || fd < 0 || fd >= MAX_FDS)
        return;

    fd_entry_t *entry = &table->fds[fd];

    // release the file descriptor resources based on the type
    switch (entry->type)
    {
    case FD_TYPE_FILE:
        if (entry->vfs_file)
            vfs_close(entry->vfs_file);
        break;

    case FD_TYPE_CONSOLE:
        break;

    case FD_TYPE_PIPE:
        if (entry->pipe)
        {
            int is_write = (entry->flags & O_WRONLY) ? 1 : 0;
            pipe_release((pipe_t *)entry->pipe, is_write);
        }
        break;

    default:
        break;
    }

    // mark as free
    memset(entry, 0, sizeof(fd_entry_t));
    entry->type = FD_TYPE_NONE;
}

int fd_table_dup(fd_table_t *table, int oldfd)
{
    fd_entry_t *old = fd_table_get(table, oldfd);
    if (!old)
        return -EBADF;

    int newfd = fd_table_alloc(table);
    if (newfd < 0)
        return -EMFILE;

    // copy the entry (struct only)
    table->fds[newfd] = *old;

    // bump reference counts for shared resources
    if (old->type == FD_TYPE_PIPE && old->pipe)
        pipe_retain((pipe_t *)old->pipe, (old->flags & O_WRONLY) ? 1 : 0);
    else if (old->type == FD_TYPE_FILE && old->vfs_file)
        vfs_retain_file(old->vfs_file);

    return newfd;
}

int fd_table_dup2(fd_table_t *table, int oldfd, int newfd)
{
    if (oldfd == newfd)
        return newfd;

    fd_entry_t *old = fd_table_get(table, oldfd);
    if (!old)
        return -EBADF;

    if (newfd < 0 || newfd >= MAX_FDS)
        return -EBADF;

    if (table->fds[newfd].type != FD_TYPE_NONE)
    {
        fd_table_free(table, newfd);
    }

    /* Copy the core entry attributes */
    table->fds[newfd].type = old->type;
    table->fds[newfd].flags = old->flags;

    if (old->type == FD_TYPE_FILE && old->vfs_file)
    {
        /* Allocate a distinct file_t so close() doesn't cause a double-free */
        file_t *new_file = kmalloc(sizeof(file_t));
        new_file->vnode = old->vfs_file->vnode;
        new_file->offset = old->vfs_file->offset;
        new_file->flags = old->vfs_file->flags;

        /* Retain the vnode so the underlying file stays open */
        vfs_retain_file(new_file);

        table->fds[newfd].vfs_file = new_file;
    }
    else if (old->type == FD_TYPE_PIPE && old->pipe)
    {
        pipe_retain((pipe_t *)old->pipe, (old->flags & O_WRONLY) ? 1 : 0);
        table->fds[newfd].pipe = old->pipe;
    }
    else
    {
        table->fds[newfd].console_type = old->console_type;
    }

    return newfd;
}