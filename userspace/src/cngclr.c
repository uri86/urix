/*
 * Licensed under MIT License - URIX project.
 * change_display.c - Interactive terminal color changer.
 */
#include "urix.h"
#include "../liburix/conf.h"
#include "string.h"

#define SHELL_CONF  "/etc/shell.conf"
#define NUM_COLORS 16

static const char *color_names[NUM_COLORS] = {
    "Black",         /* 0  */
    "Blue",          /* 1  */
    "Green",         /* 2  */
    "Cyan",          /* 3  */
    "Red",           /* 4  */
    "Magenta",       /* 5  */
    "Brown",         /* 6  */
    "Light Grey",    /* 7  */
    "Dark Grey",     /* 8  */
    "Light Blue",    /* 9  */
    "Light Green",   /* 10 */
    "Light Cyan",    /* 11 */
    "Light Red",     /* 12 */
    "Light Magenta", /* 13 */
    "Light Brown",   /* 14 */
    "White",         /* 15 */
};

/* 0 = editing foreground, 1 = editing background */
static int selected = 0;

static void render(int fg, int bg)
{
    clear_screen();

    println("=== Color Changer ===");
    println("");

    if (selected == 1)
        print("> ");
    else
        print("  ");
    print("Background : ");
    println(color_names[bg]);

    if (selected == 0)
        print("> ");
    else
        print("  ");
    print("Foreground : ");
    println(color_names[fg]);

    println("");
    println("[W/S] change  [Tab] switch  [R] reset  [Enter] save & quit  [Q] quit");
}

/*
 * save_colors - persist fg and bg into /etc/shell.conf.
 */
static void save_colors(int fg, int bg)
{
    conf_t cfg;
    char   val[8];

    conf_load(&cfg, SHELL_CONF);

    itoa(fg, val, 10);
    conf_set(&cfg, "fg", val);

    itoa(bg, val, 10);
    conf_set(&cfg, "bg", val);

    conf_save(&cfg);
}

int main(void)
{
    int fg = 7;
    int bg = 0;

    /* start from whatever is already saved */
    conf_t cfg;
    char   val[8];
    if (conf_load(&cfg, SHELL_CONF) == CONF_OK)
    {
        if (conf_get(&cfg, "fg", val, sizeof(val)) == CONF_OK) fg = atoi(val);
        if (conf_get(&cfg, "bg", val, sizeof(val)) == CONF_OK) bg = atoi(val);
    }

    change_terminal_color(fg, bg);
    render(fg, bg);

    while (1)
    {
        int c = getchar();
        if (c >= 'A' && c <= 'Z')
            c = c + ('a' - 'A');

        switch (c)
        {
        case 'w':
            if (selected == 0)
                fg = (fg + 1) % NUM_COLORS;
            else
                bg = (bg + 1) % NUM_COLORS;
            break;
        case 's':
            if (selected == 0)
                fg = (fg - 1 + NUM_COLORS) % NUM_COLORS;
            else
                bg = (bg - 1 + NUM_COLORS) % NUM_COLORS;
            break;
        case '\t': /* tab to switch between fg/bg */
            selected = !selected;
            break;
        case 'r':
            fg = 7;
            bg = 0;
            break;
        case '\r':
        case '\n':
            save_colors(fg, bg);
            clear_screen();
            exit(0);
            break;
        case 'q':
            clear_screen();
            exit(0);
            break;
        default:
            break;
        }

        change_terminal_color(fg, bg);
        render(fg, bg);
    }

    return 0;
}