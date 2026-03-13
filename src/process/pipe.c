/*
 * Licensed under MIT License - URIX project.
 * pipe.c - Pipe implementation.
 */

#include <process/pipe.h>
#include <process/process.h>
#include <memory/kmalloc.h>
#include <string.h>
#include <lib/print.h>

pipe_t *pipe_create(void)
{
    pipe_t *p = kmalloc(sizeof(pipe_t));
    if (!p)
        return NULL;

    memset(p, 0, sizeof(pipe_t));
    p->ref_count = 2; // one for reader fd, one for writer fd
    p->reader_count = 1;
    p->writer_count = 1;
    p->read_pos = 0;
    p->write_pos = 0;
    p->count = 0;

    return p;
}

void pipe_retain(pipe_t *p, int is_write_end)
{
    if (!p)
        return;
    p->ref_count++;
    if (is_write_end)
        p->writer_count++;
    else
        p->reader_count++;
}

void pipe_release(pipe_t *p, int is_write_end)
{
    if (!p)
        return;

    if (is_write_end)
        p->writer_count--;
    else
        p->reader_count--;

    p->ref_count--;
    if (p->ref_count <= 0)
    {
        kfree(p);
    }
}

int pipe_write(pipe_t *p, const void *buf, size_t count)
{
    if (!p || !buf)
        return -1;

    // prevents writing if the pipe doesn't have someone o read from the pipe.
    if (p->reader_count <= 0)
        return -1;

    const uint8_t *src = (const uint8_t *)buf;
    size_t written = 0;

    while (written < count)
    {
        // wait if the buffer is full
        while (p->count == PIPE_BUF_SIZE)
        {
            if (p->reader_count <= 0)
                return (int)written;
            process_yield();
        }

        // write one byte at a time
        p->buf[p->write_pos] = src[written];
        p->write_pos = (p->write_pos + 1) % PIPE_BUF_SIZE;
        p->count++;
        written++;
    }

    return (int)written;
}

int pipe_read(pipe_t *p, void *buf, size_t count)
{
    if (!p || !buf)
        return -1;

    uint8_t *dst = (uint8_t *)buf;
    size_t read = 0;

    while (read < count)
    {
        // if the buffer is empty check for EOF/wait.
        while (p->count == 0)
        {
            if (p->writer_count <= 0)
                return (int)read; // returns on EOF.
            process_yield();
        }

        dst[read] = p->buf[p->read_pos];
        p->read_pos = (p->read_pos + 1) % PIPE_BUF_SIZE;
        p->count--;
        read++;
    }

    return (int)read;
}