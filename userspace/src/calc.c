#include <urix.h>
#include <stdio.h>
#include "../liburix/conf.h"
#include "string.h"

#define NUM_ROWS 5
#define NUM_COLS 4

const char *buttons[NUM_ROWS][NUM_COLS] = {
    {"7", "8", "9", "/"},
    {"4", "5", "6", "*"},
    {"1", "2", "3", "-"},
    {"C", "0", "=", "+"},
    {"q", " ", " ", " "}};

long current_value = 0;
long stored_value = 0;
char stored_op = 0;
int new_number = 1;
char display[64] = "0";

int cursor_row = 0;
int cursor_col = 0;

uint64_t g_fg_color = 7;
uint64_t g_bg_color = 0;

static void load_config(void)
{
    conf_t cfg;
    char val[CONF_VAL_LEN];

    if (conf_load(&cfg, "/etc/shell.conf") == CONF_OK)
    {
        if (conf_get(&cfg, "fg_color", val, sizeof(val)) == CONF_OK)
        {
            g_fg_color = (uint64_t)atoi(val);
        }
        if (conf_get(&cfg, "bg_color", val, sizeof(val)) == CONF_OK)
        {
            g_bg_color = (uint64_t)atoi(val);
        }
    }
    change_terminal_color(g_fg_color, g_bg_color);
}

typedef struct
{
    char b[4096];
    int len;
} abuf_t;

static void ab_append(abuf_t *ab, const char *s, int len)
{
    if (ab->len + len >= (int)sizeof(ab->b))
    {
        write(STDOUT_FILENO, ab->b, ab->len);
        ab->len = 0;
    }
    memcpy(&ab->b[ab->len], s, len);
    ab->len += len;
}

static void ab_flush(abuf_t *ab)
{
    if (ab->len > 0)
    {
        write(STDOUT_FILENO, ab->b, ab->len);
        ab->len = 0;
    }
}

static void set_cursor(abuf_t *ab, int x, int y)
{
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", y + 1, x + 1);
    if (len > 0)
        ab_append(ab, buf, len);
}

static void render_screen()
{
    abuf_t ab = {0};
    ab_append(&ab, "\x1b[?25l", 6); // hide cursor
    set_cursor(&ab, 0, 0);

    ab_append(&ab, "+-----------------+\r\n", 21);

    char line[128];
    int len = snprintf(line, sizeof(line), "| %15s |\r\n", display);
    if (len > 0)
        ab_append(&ab, line, len);

    ab_append(&ab, "+-----------------+\r\n", 21);

    for (int r = 0; r < NUM_ROWS; r++)
    {
        ab_append(&ab, "|", 1);
        for (int c = 0; c < NUM_COLS; c++)
        {
            if (r == cursor_row && c == cursor_col)
            {
                ab_flush(&ab);
                change_terminal_color(g_bg_color, g_fg_color); // Invert
            }
            char btn[16];
            len = snprintf(btn, sizeof(btn), " %s ", buttons[r][c]);
            if (len > 0)
                ab_append(&ab, btn, len);
            if (r == cursor_row && c == cursor_col)
            {
                ab_flush(&ab);
                change_terminal_color(g_fg_color, g_bg_color); // Reset
            }
            if (c < NUM_COLS - 1)
            {
                ab_append(&ab, "|", 1);
            }
        }
        ab_append(&ab, "|\r\n", 3);
        if (r < NUM_ROWS - 1)
        {
            ab_append(&ab, "+---+---+---+---+\r\n", 19);
        }
    }
    ab_append(&ab, "+-----------------+\r\n", 21);

    ab_flush(&ab);
}

static void execute_op()
{
    if (stored_op == '+')
        current_value = stored_value + current_value;
    else if (stored_op == '-')
        current_value = stored_value - current_value;
    else if (stored_op == '*')
        current_value = stored_value * current_value;
    else if (stored_op == '/')
        current_value = stored_value / current_value;
}

static void handle_button(char btn)
{
    if (btn == ' ')
        return;

    if (btn == 'q')
    {
        clear_screen();
        abuf_t ab = {0};
        ab_append(&ab, "\x1b[?25h", 6); // show cursor
        ab_flush(&ab);
        exit(0);
    }

    if (btn >= '0' && btn <= '9')
    {
        if (new_number)
        {
            current_value = btn - '0';
            new_number = 0;
        }
        else
        {
            current_value = current_value * 10 + (btn - '0');
        }
        snprintf(display, sizeof(display), "%ld", current_value);
    }
    else if (btn == 'C')
    {
        current_value = 0;
        stored_value = 0;
        stored_op = 0;
        new_number = 1;
        snprintf(display, sizeof(display), "0");
    }
    else if (btn == '+' || btn == '-' || btn == '*' || btn == '/')
    {
        if (!new_number && stored_op != 0)
        {
            execute_op();
            snprintf(display, sizeof(display), "%ld", current_value);
        }
        stored_value = current_value;
        stored_op = btn;
        new_number = 1;
    }
    else if (btn == '=')
    {
        execute_op();
        stored_op = 0;
        new_number = 1;
        snprintf(display, sizeof(display), "%ld", current_value);
    }
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    load_config();
    clear_screen();
    render_screen();

    while (1)
    {
        int c = getchar();
        if (c == 3)
        { // Ctrl+C
            handle_button('q');
        }
        else if (c == 27)
        { // ESC
            int next = getchar();
            if (next == '[')
            {
                int arrow = getchar();
                if (arrow == 'A' && cursor_row > 0)
                    cursor_row--;
                if (arrow == 'B' && cursor_row < NUM_ROWS - 1)
                    cursor_row++;
                if (arrow == 'C' && cursor_col < NUM_COLS - 1)
                    cursor_col++;
                if (arrow == 'D' && cursor_col > 0)
                    cursor_col--;
                render_screen();
            }
        }
        else if (c == '\n' || c == '\r')
        {
            handle_button(buttons[cursor_row][cursor_col][0]);
            render_screen();
        }
        else if (c == 'q')
        {
            handle_button('q');
        }
        else
        {
            // direct typing
            char chr = (char)c;

            // Allow alternative keys for better UX
            if (chr == 'x' || chr == 'X')
            {
                handle_button('*');
                render_screen();
                continue;
            }
            if (chr == 'c')
            {
                handle_button('C');
                render_screen();
                continue;
            }
            if (chr == '\b' || chr == 127)
            { // backspace? (optional, ignoring for now)
                continue;
            }

            for (int r = 0; r < NUM_ROWS; r++)
            {
                for (int col = 0; col < NUM_COLS; col++)
                {
                    if (buttons[r][col][0] == chr)
                    {
                        handle_button(chr);
                        render_screen();
                        break;
                    }
                }
            }
        }
    }
    return 0;
}
