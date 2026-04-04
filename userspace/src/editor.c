/**
 * Licensed under MIT License - URIX project
 * editor.c - Simple text file editor
 * Usage: editor <filename>
 *
 * Controls:
 *   Arrow keys  - Move cursor
 *   Enter       - New line
 *   Backspace   - Delete previous character
 *   Delete      - Delete character at cursor
 *   Ctrl+S      - Save file
 *   Ctrl+Q      - Quit without saving
 */

#include <urix.h>
#include <stdio.h>
#include "../liburix/conf.h"
#include "string.h"
#include "auth.h"

#define MAX_LINES 1024
#define MAX_LINE_LEN 512
#define SCREEN_HEIGHT 24
#define SCREEN_WIDTH 80
#define ESC_CHAR 27
#define TAB_WIDTH 4

#define EDITOR_CONF "/etc/editor.conf"

typedef struct
{
    char lines[MAX_LINES][MAX_LINE_LEN];
    int num_lines;
    int cursor_line;
    int cursor_col;
    int viewport_top;
    uint64_t fg_color;
    uint64_t bg_color;
} editor_t;

static editor_t g_editor;
static char g_filename[512];

static void update_viewport(editor_t *ed)
{
    int viewport_height = SCREEN_HEIGHT - 2;
    if (ed->cursor_line < ed->viewport_top)
        ed->viewport_top = ed->cursor_line;
    else if (ed->cursor_line >= ed->viewport_top + viewport_height)
        ed->viewport_top = ed->cursor_line - viewport_height + 1;
}

static void right_shift_from(char *line, int index)
{
    size_t len = strlen(line);
    if (index < 0 || index > (int)len || len >= MAX_LINE_LEN - 1)
        return;
    memmove(line + index + 1, line + index, len - index + 1);
}

static void left_shift_from(char *line, int index)
{
    size_t len = strlen(line);
    if (index < 0 || index >= (int)len)
        return;
    memmove(line + index, line + index + 1, len - index);
}

static void remove_line(editor_t *ed, int index)
{
    if (index < 0 || index >= ed->num_lines)
        return;
    for (int i = index; i < ed->num_lines - 1; i++)
    {
        strcpy(ed->lines[i], ed->lines[i + 1]);
    }
    ed->num_lines--;
}

static void join_line_with_next(editor_t *ed, int line_idx)
{
    if (line_idx < 0 || line_idx >= ed->num_lines - 1)
        return;

    char *current = ed->lines[line_idx];
    char *next = ed->lines[line_idx + 1];
    if (strlen(current) + strlen(next) >= MAX_LINE_LEN)
        return;

    strcat(current, next);
    remove_line(ed, line_idx + 1);
}

/* Load editor config */
static void load_config(editor_t *ed)
{
    conf_t cfg;
    char val[CONF_VAL_LEN];

    ed->fg_color = 7; /* white */
    ed->bg_color = 0; /* black */

    if (conf_load(&cfg, EDITOR_CONF) == CONF_OK)
    {
        if (conf_get(&cfg, "fg_color", val, sizeof(val)) == CONF_OK)
        {
            ed->fg_color = (uint64_t)atoi(val);
        }
        if (conf_get(&cfg, "bg_color", val, sizeof(val)) == CONF_OK)
        {
            ed->bg_color = (uint64_t)atoi(val);
        }
    }

    change_terminal_color(ed->fg_color, ed->bg_color);
}

/* Load file into editor buffer */
static int load_file(editor_t *ed, const char *filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        /* File doesn't exist, create empty one */
        ed->num_lines = 1;
        ed->lines[0][0] = '\0';
        return 0;
    }

    char buf[4096];
    ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (bytes < 0)
        return -1;

    buf[bytes] = '\0';

    /* Parse lines from buffer */
    int line_idx = 0;
    int col_idx = 0;

    for (int i = 0; i < bytes && line_idx < MAX_LINES; i++)
    {
        char c = buf[i];

        if (c == '\n' || c == '\r')
        {
            ed->lines[line_idx][col_idx] = '\0';
            line_idx++;
            col_idx = 0;

            /* Handle \r\n */
            if (c == '\r' && i + 1 < bytes && buf[i + 1] == '\n')
                i++;
        }
        else if (col_idx < MAX_LINE_LEN - 1)
        {
            ed->lines[line_idx][col_idx++] = c;
        }
    }

    ed->lines[line_idx][col_idx] = '\0';
    ed->num_lines = line_idx + 1;

    return 0;
}

/* Save file */
static int save_file(editor_t *ed, const char *filename)
{
    /* Create new file with all lines, safely truncating existing if any */
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0)
        return -1;

    for (int i = 0; i < ed->num_lines; i++)
    {
        write(fd, ed->lines[i], strlen(ed->lines[i]));
        if (i < ed->num_lines - 1)
        {
            write(fd, "\n", 1);
        }
    }

    close(fd);
    return 0;
}

/* Insert character at current cursor position */
static void insert_char(editor_t *ed, char c)
{
    if (ed->cursor_line >= MAX_LINES)
        return;

    char *line = ed->lines[ed->cursor_line];
    int len = strlen(line);

    if (len >= MAX_LINE_LEN - 1)
        return;

    right_shift_from(line, ed->cursor_col);
    line[ed->cursor_col] = c;
    ed->cursor_col++;
}

/* Delete character at current cursor position (backspace) */
static void backspace(editor_t *ed)
{
    if (ed->cursor_col == 0 && ed->cursor_line == 0)
        return;

    if (ed->cursor_col > 0)
    {
        /* Delete in current line */
        left_shift_from(ed->lines[ed->cursor_line], ed->cursor_col - 1);
        ed->cursor_col--;
    }
    else
    {
        if (ed->cursor_line == 0)
            return;

        ed->cursor_line--;
        ed->cursor_col = strlen(ed->lines[ed->cursor_line]);
        join_line_with_next(ed, ed->cursor_line);
    }

    update_viewport(ed);
}

/* Delete character at cursor (forward delete) */
static void delete_char(editor_t *ed)
{
    char *line = ed->lines[ed->cursor_line];

    if (ed->cursor_col >= (int)strlen(line))
    {
        if (ed->cursor_line < ed->num_lines - 1)
            join_line_with_next(ed, ed->cursor_line);
        return;
    }

    left_shift_from(line, ed->cursor_col);
}

/* Create new line (split at cursor) */
static void create_newline(editor_t *ed)
{
    if (ed->num_lines >= MAX_LINES)
        return;

    char *current = ed->lines[ed->cursor_line];

    /* Shift lines down */
    for (int i = ed->num_lines; i > ed->cursor_line + 1; i--)
    {
        strcpy(ed->lines[i], ed->lines[i - 1]);
    }

    strcpy(ed->lines[ed->cursor_line + 1], &current[ed->cursor_col]);
    current[ed->cursor_col] = '\0';

    ed->cursor_line++;
    ed->cursor_col = 0;
    ed->num_lines++;

    update_viewport(ed);
}

typedef struct
{
    char b[4096];
    int len;
} abuf_t;

static void ab_append(abuf_t *ab, const char *s, int len)
{
    if ((unsigned long)(ab->len + len) >= sizeof(ab->b))
    {
        write(STDOUT_FILENO, ab->b, ab->len);
        ab->len = 0;
    }
    memcpy(&ab->b[ab->len], s, len);
    ab->len += len;
}

static void ab_putchar(abuf_t *ab, char c)
{
    if ((unsigned long)ab->len >= sizeof(ab->b))
    {
        write(STDOUT_FILENO, ab->b, ab->len);
        ab->len = 0;
    }
    ab->b[ab->len++] = c;
}

static void ab_flush(abuf_t *ab)
{
    if (ab->len > 0)
    {
        write(STDOUT_FILENO, ab->b, ab->len);
        ab->len = 0;
    }
}

static void append_uint(abuf_t *ab, unsigned long value)
{
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%lu", value);
    if (len > 0)
        ab_append(ab, buf, len);
}

/* Set the cursor position natively using ANSI escape sequences */
static void set_cursor(abuf_t *ab, int x, int y)
{
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", y + 1, x + 1);
    if (len > 0)
        ab_append(ab, buf, len);
}

/* Render entire screen */
static void render_screen(editor_t *ed)
{
    abuf_t ab = {0};
    ab_append(&ab, "\x1b[?25l", 6); // hide cursor

    int viewport_height = SCREEN_HEIGHT - 2;

    /* Display lines */
    for (int i = 0; i < viewport_height; i++)
    {
        set_cursor(&ab, 0, i);
        if (ed->viewport_top + i < ed->num_lines)
        {
            char *line = ed->lines[ed->viewport_top + i];

            ab_append(&ab, "  ", 2);

            int j = 0;
            size_t llen = strlen(line);
            for (; (size_t)j < llen && j < SCREEN_WIDTH - 4; j++)
            {
                ab_putchar(&ab, line[j]);
            }

            /* Pad remaining spaces to end of row */
            for (; j < SCREEN_WIDTH - 2; j++)
            {
                ab_putchar(&ab, ' ');
            }
        }
        else
        {
            /* Empty padding */
            ab_append(&ab, "~ ", 2);
            for (int j = 0; j < SCREEN_WIDTH - 2; j++)
            {
                ab_putchar(&ab, ' ');
            }
        }
    }

    /* Status bar */
    set_cursor(&ab, 0, viewport_height);
    ab_append(&ab, "---", 3);
    for (int j = 3; j < SCREEN_WIDTH; j++)
        ab_putchar(&ab, ' ');

    set_cursor(&ab, 0, viewport_height + 1);
    ab_append(&ab, "Line: ", 6);

    /* Print line number and column number */
    append_uint(&ab, ed->cursor_line + 1);
    ab_append(&ab, " Col: ", 6);
    append_uint(&ab, ed->cursor_col + 1);

    ab_append(&ab, " | Ctrl+S to save, Ctrl+Q to quit", 33);
    for (int j = 0; j < 20; j++)
        ab_putchar(&ab, ' ');

    set_cursor(&ab, ed->cursor_col + 2, ed->cursor_line - ed->viewport_top);
    ab_append(&ab, "\x1b[?25h", 6);

    ab_flush(&ab);
}

/* Move cursor, handling viewport */
static void move_cursor(editor_t *ed, int dx, int dy)
{
    ed->cursor_col += dx;
    ed->cursor_line += dy;

    /* Bounds checking */
    if (ed->cursor_line < 0)
        ed->cursor_line = 0;
    if (ed->cursor_line >= ed->num_lines)
        ed->cursor_line = ed->num_lines - 1;

    /* Keep cursor within line */
    int max_col = strlen(ed->lines[ed->cursor_line]);
    if (ed->cursor_col > max_col)
        ed->cursor_col = max_col;
    if (ed->cursor_col < 0)
        ed->cursor_col = 0;

    /* Adjust viewport to keep cursor visible */
    update_viewport(ed);
    render_screen(ed);
}

/* Main editor loop */
static int editor_loop(editor_t *ed)
{
    render_screen(ed);

    while (1)
    {
        int c = getchar();

        if (c == 3)
        {
            /* Ctrl+C */
            break;
        }
        else if (c == 19)
        {
            /* Ctrl+S - Save */
            if (save_file(ed, g_filename) == 0)
            {
                print("\n[Saved]");
            }
            else
            {
                print("\n[Error saving file]");
            }
            render_screen(ed);
        }
        else if (c == 17)
        {
            /* Ctrl+Q - Quit */
            break;
        }
        else if (c == 127 || c == 8)
        {
            /* Backspace */
            backspace(ed);
            render_screen(ed);
        }
        else if (c == ESC_CHAR)
        {
            /* Arrow keys and special keys */
            int next = getchar();
            if (next == '[')
            {
                int arrow = getchar();

                if (arrow == 'A')
                {
                    /* Up arrow */
                    move_cursor(ed, 0, -1);
                }
                else if (arrow == 'B')
                {
                    /* Down arrow */
                    move_cursor(ed, 0, 1);
                }
                else if (arrow == 'C')
                {
                    /* Right arrow */
                    move_cursor(ed, 1, 0);
                }
                else if (arrow == 'D')
                {
                    /* Left arrow */
                    move_cursor(ed, -1, 0);
                }
                else if (arrow == '3')
                {
                    /* Delete key */
                    getchar(); /* consume ~ */
                    delete_char(ed);
                    render_screen(ed);
                }
            }
        }
        else if (c == '\n' || c == '\r')
        {
            /* Enter - new line */
            create_newline(ed);
            render_screen(ed);
        }
        else if (c == '\t')
        {
            /* Tab key - insert spaces */
            for (int i = 0; i < TAB_WIDTH; i++)
            {
                insert_char(ed, ' ');
            }
            render_screen(ed);
        }
        else if (c >= 32 && c <= 126)
        {
            /* Printable character */
            insert_char(ed, c);
            render_screen(ed);
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        println("Usage: editor <filename>");
        exit(1);
    }

    strcpy(g_filename, argv[1]);

    if (is_session_file(g_filename)) {
        println("Access denied: Session file cannot be manually edited.");
        exit(1);
    }
    
    require_root_for_ps(g_filename);

    /* Initialize editor */
    g_editor.cursor_line = 0;
    g_editor.cursor_col = 0;
    g_editor.viewport_top = 0;
    g_editor.num_lines = 0;

    /* Load config */
    load_config(&g_editor);

    /* Load file */
    if (load_file(&g_editor, g_filename) != 0)
    {
        println("Error loading file");
        exit(1);
    }

    /* Clear the screen initially to provide a clean state */
    clear_screen();

    /* Run editor */
    editor_loop(&g_editor);

    /* Clear screen on exit */
    clear_screen();

    exit(0);
    return 0;
}
