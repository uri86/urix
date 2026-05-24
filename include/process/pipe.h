/*
 * Licensed under MIT License - URIX project.
 * pipe.h - pipe interface definitions.
 * Responsibilities:
 *  - define the pipe data structure
 *  - provide the definition of pipe functions.
 */

#ifndef PIPE_H
#define PIPE_H

#include <stdint.h>
#include <stddef.h>

#define PIPE_BUF_SIZE 4096

/*
 * Pipe flags stored in fd_entry.flags to distinguish the two ends.
 */
#define PIPE_READ 0
#define PIPE_WRITE 1


typedef struct pipe
{
    uint8_t buf[PIPE_BUF_SIZE]; // data buffer
    uint32_t read_pos; // the next byte to read
    uint32_t write_pos; // the next byte to write to
    uint32_t count;
    int ref_count;
    int reader_count;
    int writer_count;
} pipe_t;

/**
 * pipe_create - Allocate and initialize a new pipe.
 *
 * Returns: pointer to the new pipe_t, or NULL on failure.
 */
pipe_t *pipe_create(void);

/**
 * pipe_retain - Increment the reference count.
 *
 * is_write_end: 1 if retaining a write-end fd, 0 for read-end.
 */
void pipe_retain(pipe_t *p, int is_write_end);

/**
 * pipe_release - Decrement the reference count and free if zero.
 *
 * is_write_end: 1 if the caller is releasing the write end, 0 if releasing the read end.
 */
void pipe_release(pipe_t *p, int is_write_end);

/**
 * pipe_write - Write bytes into the pipe ring buffer.
 *
 * Blocks if the buffer is full.
 * Returns number of bytes written, or -1 if the read end is closed.
 */
int pipe_write(pipe_t *p, const void *buf, size_t count);

/**
 * pipe_read - Read bytes from the pipe ring buffer.
 *
 * Blocks if the buffer is empty and the
 * write end is still open.
 * Returns number of bytes read, or 0 on EOF (write end closed + empty).
 */
int pipe_read(pipe_t *p, void *buf, size_t count);

#endif /* PIPE_H */