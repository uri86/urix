/*
 * Licensed under MIT License - URIX project.
 * conf.h - Simple key=value config files.
 */

#ifndef CONF_H
#define CONF_H

#include <stddef.h>
#include "urix.h"

#ifndef CONF_MAX_ENTRIES
#define CONF_MAX_ENTRIES 64
#endif

#ifndef CONF_KEY_LEN
#define CONF_KEY_LEN 64
#endif

#ifndef CONF_VAL_LEN
#define CONF_VAL_LEN 128
#endif

#ifndef CONF_PATH_LEN
#define CONF_PATH_LEN 256
#endif

#define CONF_OK 0
#define CONF_ERR -1 /* I/O error */
#define CONF_NOTFOUND -2 /* key does not exist */
#define CONF_FULL -3 /* no space for entry */
#define CONF_TOOLONG -4 /* key or value too long */

typedef struct
{
    char key[CONF_KEY_LEN];
    char value[CONF_VAL_LEN];
} conf_entry_t;

typedef struct
{
    conf_entry_t entries[CONF_MAX_ENTRIES];
    int count;
    char path[CONF_PATH_LEN];
} conf_t;

/*
 * conf_load - Load a config file into cfg.
 *
 * Creates the file if it does not exist.
 * Returns CONF_OK or CONF_ERR.
 */
int conf_load(conf_t *cfg, const char *path);

/*
 * conf_get - Get the value of a key.
 *
 * Writes the value into out (up to out_len bytes).
 * Returns CONF_OK, CONF_NOTFOUND, or CONF_TOOLONG.
 */
int conf_get(const conf_t *cfg, const char *key, char *out, size_t out_len);

/*
 * conf_set - Set a key to a value, adding it if it doesn't exist.
 *
 * Returns CONF_OK, CONF_FULL, or CONF_TOOLONG.
 */
int conf_set(conf_t *cfg, const char *key, const char *value);

/*
 * conf_delete - Remove a key from the config.
 *
 * Returns CONF_OK or CONF_NOTFOUND.
 */
int conf_delete(conf_t *cfg, const char *key);

/*
 * conf_save - Write the in-memory config back to its file.
 *
 * Returns CONF_OK or CONF_ERR.
 */
int conf_save(const conf_t *cfg);

#endif /* CONF_H */