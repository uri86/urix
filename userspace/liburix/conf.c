/*
 * Licensed under MIT License - URIX project.
 * conf.c - Simple key=value config files.
 *
 * File format (one entry per line):
 *   key=value
 *   # lines starting with # are comments and are ignored
 *   blank lines are ignored
 */

#include "conf.h"
#include "string.h"

static int find_key(const conf_t *cfg, const char *key)
{
    int i;
    for (i = 0; i < cfg->count; i++)
        if (strcmp(cfg->entries[i].key, key) == 0)
            return i;
    return -1;
}

static int read_line(int fd, char *buf, size_t buf_size)
{
    size_t n = 0;
    int got = 0;
    char c;

    while (n < buf_size - 1)
    {
        ssize_t r = read(fd, &c, 1);
        if (r < 0)
            return -1;
        if (r == 0)
            break; /* EOF */
        got = 1;
        if (c == '\n')
            break;
        buf[n++] = c;
    }

    buf[n] = '\0';
    return got ? 1 : 0; /* 0 = true EOF, 1 = got a line */
}

static int write_str(int fd, const char *s)
{
    size_t len = strlen(s);
    return write(fd, s, len) == (ssize_t)len ? CONF_OK : CONF_ERR;
}

/* strip leading whitespace, return pointer into s */
static const char *ltrim(const char *s)
{
    while (*s == ' ' || *s == '\t' || *s == '\r')
        s++;
    return s;
}

static void rtrim(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

int conf_load(conf_t *cfg, const char *path)
{
    if (!cfg || !path)
        return CONF_ERR;

    memset(cfg, 0, sizeof(conf_t));
    strncpy(cfg->path, path, CONF_PATH_LEN - 1);

    int fd = open(path, O_RDWR | O_CREAT);
    if (fd < 0)
    {
        return CONF_OK;
    }

    char line[CONF_KEY_LEN + CONF_VAL_LEN + 2];

    while (1)
    {
        int r = read_line(fd, line, sizeof(line));
        if (r < 0)
        {
            close(fd);
            return CONF_ERR;
        }
        if (r == 0)
            break; /* EOF */

        const char *p = ltrim(line);

        if (*p == '\0' || *p == '#')
            continue;

        /* find the '=' */
        char *eq = strchr(p, '=');
        if (!eq)
            continue; /* malformed line, skip it */

        /* key */
        size_t klen = (size_t)(eq - p);
        if (klen == 0 || klen >= CONF_KEY_LEN)
            continue;

        if (cfg->count >= CONF_MAX_ENTRIES)
        {
            close(fd);
            return CONF_FULL;
        }

        conf_entry_t *e = &cfg->entries[cfg->count];
        memcpy(e->key, p, klen);
        e->key[klen] = '\0';
        rtrim(e->key);

        /* value */
        const char *vstart = ltrim(eq + 1);
        size_t vlen = strlen(vstart);
        if (vlen >= CONF_VAL_LEN)
        {
            close(fd);
            return CONF_TOOLONG;
        }
        memcpy(e->value, vstart, vlen + 1);
        rtrim(e->value);

        cfg->count++;
    }

    close(fd);
    return CONF_OK;
}

int conf_get(const conf_t *cfg, const char *key, char *out, size_t out_len)
{
    if (!cfg || !key || !out)
        return CONF_ERR;

    int i = find_key(cfg, key);
    if (i < 0)
        return CONF_NOTFOUND;

    size_t vlen = strlen(cfg->entries[i].value);
    if (vlen >= out_len)
        return CONF_TOOLONG;

    memcpy(out, cfg->entries[i].value, vlen + 1);
    return CONF_OK;
}

int conf_set(conf_t *cfg, const char *key, const char *value)
{
    if (!cfg || !key || !value)
        return CONF_ERR;
    if (strlen(key) >= CONF_KEY_LEN)
        return CONF_TOOLONG;
    if (strlen(value) >= CONF_VAL_LEN)
        return CONF_TOOLONG;

    int i = find_key(cfg, key);
    if (i >= 0)
    {
        /* update existing */
        strncpy(cfg->entries[i].value, value, CONF_VAL_LEN - 1);
        cfg->entries[i].value[CONF_VAL_LEN - 1] = '\0';
        return CONF_OK;
    }

    if (cfg->count >= CONF_MAX_ENTRIES)
        return CONF_FULL;

    conf_entry_t *e = &cfg->entries[cfg->count++];
    strncpy(e->key, key, CONF_KEY_LEN - 1);
    strncpy(e->value, value, CONF_VAL_LEN - 1);
    e->key[CONF_KEY_LEN - 1] = '\0';
    e->value[CONF_VAL_LEN - 1] = '\0';
    return CONF_OK;
}

int conf_delete(conf_t *cfg, const char *key)
{
    if (!cfg || !key)
        return CONF_ERR;

    int i = find_key(cfg, key);
    if (i < 0)
        return CONF_NOTFOUND;

    /* fill the gap by shifting entries down */
    int j;
    for (j = i; j < cfg->count - 1; j++)
        cfg->entries[j] = cfg->entries[j + 1];

    memset(&cfg->entries[--cfg->count], 0, sizeof(conf_entry_t));
    return CONF_OK;
}

int conf_save(const conf_t *cfg)
{
    if (!cfg || cfg->path[0] == '\0')
        return CONF_ERR;

    /* unlink first so we get a clean file */
    unlink(cfg->path);

    int fd = open(cfg->path, O_RDWR | O_CREAT);
    if (fd < 0)
        return CONF_ERR;

    int i;
    for (i = 0; i < cfg->count; i++)
    {
        if (write_str(fd, cfg->entries[i].key) != CONF_OK)
            goto err;
        if (write_str(fd, "=") != CONF_OK)
            goto err;
        if (write_str(fd, cfg->entries[i].value) != CONF_OK)
            goto err;
        if (write_str(fd, "\n") != CONF_OK)
            goto err;
    }

    close(fd);
    return CONF_OK;

err:
    close(fd);
    return CONF_ERR;
}