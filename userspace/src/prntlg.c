/**
 * Licensed under MIT License - URIX project
 * prntlg.c - print kernel log
 */
#include "urix.h"
#include "auth.h"
#include "../liburix/conf.h"

#define SHELL_CONF "/etc/shell.conf"
#define DEFAULT_FG 7
#define DEFAULT_BG 0

static void load_colors(uint64_t *fg, uint64_t *bg)
{
    conf_t cfg;
    char val[CONF_VAL_LEN];

    *fg = DEFAULT_FG;
    *bg = DEFAULT_BG;

    if (conf_load(&cfg, SHELL_CONF) != CONF_OK)
        return;

    if (conf_get(&cfg, "fg", val, sizeof(val)) == CONF_OK)
        *fg = (uint64_t)atoi(val);

    if (conf_get(&cfg, "bg", val, sizeof(val)) == CONF_OK)
        *bg = (uint64_t)atoi(val);
}

int main()
{
    require_root();
    uint64_t fg, bg;
    load_colors(&fg, &bg);
    prntlg();
    change_terminal_color(fg, bg);
    return 0;
}
