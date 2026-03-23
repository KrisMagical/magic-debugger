/**
 * @file tui_panel.c
 * @brief TUI Panel Implementations
 *
 * This file implements the various panels for the TUI subsystem,
 * providing interactive views for source code, breakpoints, call stack,
 * variables, and other debugging information.
 *
 * Phase 5: Frontend - TUI Panels
 */

#include "tui_panel.h"
#include "adapter.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* ncurses headers - optional for compilation */
#ifdef HAVE_NCURSES
#include <ncurses.h>
#else
/* Stub definitions for building without ncurses - same as tui.c */
typedef struct WINDOW WINDOW;
typedef struct PANEL PANEL;
#define KEY_UP      0x0100
#define KEY_DOWN    0x0101
#define KEY_LEFT    0x0102
#define KEY_RIGHT   0x0103
#define KEY_HOME    0x0104
#define KEY_END     0x0105
#define KEY_PPAGE   0x0106
#define KEY_NPAGE   0x0107
#define KEY_IC      0x0108
#define KEY_DC      0x0109
#define KEY_BTAB    0x010A
#define KEY_BACKSPACE 0x010B
#define KEY_F(n)    (0x0200 + (n) - 1)
#define KEY_RESIZE  0x0400
#define KEY_MOUSE   0x0300
#define ERR         (-1)
#define TRUE        1
#define FALSE       0
#define OK          0
#define A_BOLD      0x01000000
#define A_NORMAL    0
#define ACS_RARROW  '>'
#define ACS_LARROW  '<'
#define ACS_CKBOARD '#'
#define COLOR_PAIR(n) (n)
static inline WINDOW* newwin(int h, int w, int y, int x) { (void)h; (void)w; (void)y; (void)x; return NULL; }
static inline int wmove(WINDOW *w, int y, int x) { (void)w; (void)y; (void)x; return 0; }
static inline int waddch(WINDOW *w, int c) { (void)w; (void)c; return 0; }
static inline int wattron(WINDOW *w, int a) { (void)w; (void)a; return 0; }
static inline int wattroff(WINDOW *w, int a) { (void)w; (void)a; return 0; }
static inline int mvwprintw(WINDOW *w, int y, int x, const char *fmt, ...) { (void)w; (void)y; (void)x; (void)fmt; return 0; }
static inline int mvwaddch(WINDOW *w, int y, int x, int c) { (void)w; (void)y; (void)x; (void)c; return 0; }
#endif

/* ============================================================================
 * Common Panel Helpers
 * ============================================================================ */

/**
 * @brief Draw panel content line with color
 */
static void draw_line(WINDOW *win, int y, int x, const char *text,
                      int max_len, int color_pair, bool bold)
{
    wmove(win, y, x);
    if (bold) {
        wattron(win, A_BOLD);
    }
    wattron(win, COLOR_PAIR(color_pair));

    /* Print text with length limit */
    int len = strlen(text);
    int print_len = (len < max_len) ? len : max_len;
    for (int i = 0; i < print_len; i++) {
        waddch(win, text[i]);
    }

    /* Clear rest of line */
    for (int i = print_len; i < max_len; i++) {
        waddch(win, ' ');
    }

    wattroff(win, COLOR_PAIR(color_pair));
    if (bold) {
        wattroff(win, A_BOLD);
    }
}

/**
 * @brief Truncate string to fit width with ellipsis
 */
static void truncate_string(const char *src, char *dst, int max_len)
{
    int len = strlen(src);
    if (len <= max_len) {
        strcpy(dst, src);
        return;
    }

    if (max_len <= 3) {
        strncpy(dst, src, max_len);
        dst[max_len] = '\0';
        return;
    }

    strncpy(dst, src, max_len - 3);
    dst[max_len - 3] = '.';
    dst[max_len - 2] = '.';
    dst[max_len - 1] = '.';
    dst[max_len] = '\0';
}

/* ============================================================================
 * Source Panel Implementation
 * ============================================================================ */

/* Forward declarations */
static md_error_t source_panel_init(md_tui_panel_t *panel);
static void source_panel_destroy(md_tui_panel_t *panel);
static void source_panel_render(md_tui_panel_t *panel);
static bool source_panel_handle_key(md_tui_panel_t *panel, int key);
static void source_panel_on_resize(md_tui_panel_t *panel, const md_tui_rect_t *rect);

static md_tui_panel_interface_t s_source_panel_vtable = {
    .init = source_panel_init,
    .destroy = source_panel_destroy,
    .render = source_panel_render,
    .handle_key = source_panel_handle_key,
    .on_resize = source_panel_on_resize,
};

static md_error_t source_panel_init(md_tui_panel_t *panel)
{
    md_source_panel_data_t *data = calloc(1, sizeof(md_source_panel_data_t));
    if (!data) return MD_ERROR_OUT_OF_MEMORY;

    data->top_line = 1;
    data->cursor_line = 1;
    data->syntax_enabled = true;

    panel->user_data = data;
    return MD_SUCCESS;
}

static void source_panel_destroy(md_tui_panel_t *panel)
{
    md_source_panel_data_t *data = (md_source_panel_data_t*)panel->user_data;
    if (data) {
        /* Free line array */
        if (data->lines) {
            for (int i = 0; i < data->line_count; i++) {
                free(data->lines[i]);
            }
            free(data->lines);
        }
        free(data->breakpoint_lines);
        free(data->breakpoint_ids);
        free(data->bookmarks);
        free(data);
    }
}

static void source_panel_render(md_tui_panel_t *panel)
{
    WINDOW *win = (WINDOW*)panel->window;
    md_source_panel_data_t *data = (md_source_panel_data_t*)panel->user_data;
    if (!data || !win) return;

    int width = panel->rect.width - 2;   /* Account for border */
    int height = panel->rect.height - 2;
    int line_num_width = 6;              /* Space for line numbers */
    int marker_width = 2;                /* Space for breakpoint marker */
    int text_width = width - line_num_width - marker_width;

    /* Render each visible line */
    for (int i = 0; i < height; i++) {
        int line_num = data->top_line + i;
        int y = i + 1;  /* +1 for border */

        if (line_num <= data->line_count && data->lines && line_num > 0) {
            /* Line number */
            char num_buf[16];
            snprintf(num_buf, sizeof(num_buf), "%5d ", line_num);

            /* Check for breakpoint */
            bool has_breakpoint = false;
            bool bp_enabled = true;
            for (int bp = 0; bp < data->breakpoint_count; bp++) {
                if (data->breakpoint_lines[bp] == line_num) {
                    has_breakpoint = true;
                    bp_enabled = (data->breakpoint_ids[bp] > 0);
                    break;
                }
            }

            /* Is current line? */
            bool is_current = (line_num == data->current_line);

            /* Is cursor line? */
            bool is_cursor = (line_num == data->cursor_line);

            /* Build marker */
            char marker[8];
            if (is_current) {
                strcpy(marker, ">");
            } else if (has_breakpoint) {
                strcpy(marker, bp_enabled ? "B" : "b");
            } else {
                strcpy(marker, " ");
            }

            /* Draw line number and marker */
            mvwprintw(win, y, 1, "%s%s", num_buf, marker);

            /* Get line content */
            const char *line_text = data->lines[line_num - 1];
            char display_text[MD_SOURCE_LINE_MAX];
            truncate_string(line_text, display_text, text_width);

            /* Determine color */
            int color = MD_TUI_COLOR_NORMAL;
            if (is_current) {
                color = MD_TUI_COLOR_CURRENT_LINE;
            } else if (has_breakpoint) {
                color = bp_enabled ? MD_TUI_COLOR_BREAKPOINT : MD_TUI_COLOR_DISABLED;
            }

            /* Draw text */
            draw_line(win, y, line_num_width + marker_width + 1,
                     display_text, text_width, color, is_cursor);
        } else {
            /* Empty line */
            wmove(win, y, 1);
            for (int x = 0; x < width; x++) {
                waddch(win, ' ');
            }
        }
    }

    /* Draw scroll indicator */
    if (data->line_count > height) {
        int scroll_pos = (data->top_line - 1) * (height - 2) / data->line_count;
        mvwaddch(win, scroll_pos + 1, width + 1, ACS_CKBOARD);
    }
}

static bool source_panel_handle_key(md_tui_panel_t *panel, int key)
{
    md_source_panel_data_t *data = (md_source_panel_data_t*)panel->user_data;
    if (!data) return false;

    int height = panel->rect.height - 2;

    switch (key) {
        case MD_KEY_UP:
        case 'k':
            if (data->cursor_line > 1) {
                data->cursor_line--;
                if (data->cursor_line < data->top_line) {
                    data->top_line = data->cursor_line;
                }
            }
            return true;

        case MD_KEY_DOWN:
        case 'j':
            if (data->cursor_line < data->line_count) {
                data->cursor_line++;
                if (data->cursor_line >= data->top_line + height) {
                    data->top_line = data->cursor_line - height + 1;
                }
            }
            return true;

        case MD_KEY_PAGE_UP:
            data->cursor_line -= height;
            if (data->cursor_line < 1) data->cursor_line = 1;
            data->top_line -= height;
            if (data->top_line < 1) data->top_line = 1;
            return true;

        case MD_KEY_PAGE_DOWN:
            data->cursor_line += height;
            if (data->cursor_line > data->line_count) {
                data->cursor_line = data->line_count;
            }
            data->top_line += height;
            if (data->top_line + height > data->line_count) {
                data->top_line = data->line_count - height + 1;
                if (data->top_line < 1) data->top_line = 1;
            }
            return true;

        case MD_KEY_HOME:
            data->cursor_line = 1;
            data->top_line = 1;
            return true;

        case MD_KEY_END:
            data->cursor_line = data->line_count;
            data->top_line = data->line_count - height + 1;
            if (data->top_line < 1) data->top_line = 1;
            return true;

        case 'G':
            /* Go to line (would prompt for line number) */
            return true;

        case 'g':
            /* Go to current execution line */
            if (data->current_line > 0) {
                md_source_panel_scroll_to_line(panel, data->current_line, true);
                data->cursor_line = data->current_line;
            }
            return true;

        case MD_TUI_CMD_TOGGLE_BREAKPOINT:
        case 'b':
            /* Toggle breakpoint at cursor line */
            if (panel->tui && panel->tui->adapter) {
                md_adapter_set_breakpoint(panel->tui->adapter,
                                          data->file_path, data->cursor_line,
                                          NULL, NULL);
            }
            return true;

        case '/':
            /* Start search */
            return true;

        case 'n':
            /* Find next */
            if (data->search_pattern[0] != '\0') {
                md_source_panel_find(panel, data->search_pattern, false);
            }
            return true;

        case 'N':
            /* Find previous */
            if (data->search_pattern[0] != '\0') {
                md_source_panel_find(panel, data->search_pattern, true);
            }
            return true;
    }

    return false;
}

static void source_panel_on_resize(md_tui_panel_t *panel, const md_tui_rect_t *rect)
{
    md_source_panel_data_t *data = (md_source_panel_data_t*)panel->user_data;
    if (data) {
        /* Adjust view to keep cursor visible */
        int height = rect->height - 2;
        if (data->cursor_line < data->top_line) {
            data->top_line = data->cursor_line;
        } else if (data->cursor_line >= data->top_line + height) {
            data->top_line = data->cursor_line - height + 1;
        }
    }
}

MD_API md_tui_panel_t* md_source_panel_create(void)
{
    md_tui_panel_t *panel = calloc(1, sizeof(md_tui_panel_t));
    if (!panel) return NULL;

    panel->type = MD_TUI_PANEL_SOURCE;
    strcpy(panel->title, "Source");
    panel->vtable = &s_source_panel_vtable;
    panel->resizable = true;
    panel->visibility = MD_TUI_VISIBLE;

    return panel;
}

MD_API md_error_t md_source_panel_load_file(md_tui_panel_t *panel, const char *path)
{
    md_source_panel_data_t *data = (md_source_panel_data_t*)panel->user_data;
    if (!data || !path) return MD_ERROR_NULL_POINTER;

    /* Free existing lines */
    if (data->lines) {
        for (int i = 0; i < data->line_count; i++) {
            free(data->lines[i]);
        }
        free(data->lines);
        data->lines = NULL;
    }
    data->line_count = 0;

    /* Open file */
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return MD_ERROR_NOT_FOUND;
    }

    /* Count lines first */
    int count = 0;
    char buffer[MD_SOURCE_LINE_MAX];
    while (fgets(buffer, sizeof(buffer), fp)) {
        count++;
    }

    /* Allocate line array */
    data->lines = calloc(count, sizeof(char*));
    if (!data->lines) {
        fclose(fp);
        return MD_ERROR_OUT_OF_MEMORY;
    }

    /* Read lines */
    rewind(fp);
    int idx = 0;
    while (fgets(buffer, sizeof(buffer), fp) && idx < count) {
        /* Remove trailing newline */
        int len = strlen(buffer);
        while (len > 0 && (buffer[len-1] == '\n' || buffer[len-1] == '\r')) {
            buffer[--len] = '\0';
        }

        data->lines[idx] = md_strdup(buffer);
        if (data->lines[idx]) {
            idx++;
        }
    }

    fclose(fp);

    data->line_count = idx;
    strncpy(data->file_path, path, sizeof(data->file_path) - 1);

    /* Reset view */
    data->top_line = 1;
    data->cursor_line = 1;

    return MD_SUCCESS;
}

MD_API void md_source_panel_set_current_line(md_tui_panel_t *panel, int line)
{
    md_source_panel_data_t *data = (md_source_panel_data_t*)panel->user_data;
    if (!data) return;

    data->current_line = line;

    /* Scroll to show line if following execution */
    if (panel->tui && panel->tui->config.follow_exec && line > 0) {
        md_source_panel_scroll_to_line(panel, line, true);
    }
}

MD_API void md_source_panel_scroll_to_line(md_tui_panel_t *panel, int line, bool center)
{
    md_source_panel_data_t *data = (md_source_panel_data_t*)panel->user_data;
    if (!data || line < 1 || line > data->line_count) return;

    int height = panel->rect.height - 2;

    if (center) {
        /* Center line in view */
        data->top_line = line - height / 2;
    } else {
        /* Just make visible */
        if (line < data->top_line) {
            data->top_line = line;
        } else if (line >= data->top_line + height) {
            data->top_line = line - height + 1;
        }
    }

    /* Clamp to valid range */
    if (data->top_line < 1) data->top_line = 1;
    if (data->top_line + height > data->line_count + 1) {
        data->top_line = data->line_count - height + 1;
        if (data->top_line < 1) data->top_line = 1;
    }
}

MD_API int md_source_panel_find(md_tui_panel_t *panel,
                                 const char *pattern, bool backward)
{
    md_source_panel_data_t *data = (md_source_panel_data_t*)panel->user_data;
    if (!data || !pattern || data->line_count == 0) return -1;

    strncpy(data->search_pattern, pattern, sizeof(data->search_pattern) - 1);

    int start = data->cursor_line;
    int found_line = -1;

    if (backward) {
        for (int i = start - 1; i >= 1; i--) {
            if (data->lines[i-1] && strstr(data->lines[i-1], pattern)) {
                found_line = i;
                break;
            }
        }
        /* Wrap around */
        if (found_line < 0) {
            for (int i = data->line_count; i > start; i--) {
                if (data->lines[i-1] && strstr(data->lines[i-1], pattern)) {
                    found_line = i;
                    break;
                }
            }
        }
    } else {
        for (int i = start + 1; i <= data->line_count; i++) {
            if (data->lines[i-1] && strstr(data->lines[i-1], pattern)) {
                found_line = i;
                break;
            }
        }
        /* Wrap around */
        if (found_line < 0) {
            for (int i = 1; i < start; i++) {
                if (data->lines[i-1] && strstr(data->lines[i-1], pattern)) {
                    found_line = i;
                    break;
                }
            }
        }
    }

    if (found_line > 0) {
        data->search_line = found_line;
        data->cursor_line = found_line;
        md_source_panel_scroll_to_line(panel, found_line, false);
    }

    return found_line;
}

/* ============================================================================
 * Breakpoint Panel Implementation
 * ============================================================================ */

static md_error_t breakpoint_panel_init(md_tui_panel_t *panel);
static void breakpoint_panel_destroy(md_tui_panel_t *panel);
static void breakpoint_panel_render(md_tui_panel_t *panel);
static bool breakpoint_panel_handle_key(md_tui_panel_t *panel, int key);

static md_tui_panel_interface_t s_breakpoint_panel_vtable = {
    .init = breakpoint_panel_init,
    .destroy = breakpoint_panel_destroy,
    .render = breakpoint_panel_render,
    .handle_key = breakpoint_panel_handle_key,
};

static md_error_t breakpoint_panel_init(md_tui_panel_t *panel)
{
    md_breakpoint_panel_data_t *data = calloc(1, sizeof(md_breakpoint_panel_data_t));
    if (!data) return MD_ERROR_OUT_OF_MEMORY;

    data->show_disabled = true;
    panel->user_data = data;
    return MD_SUCCESS;
}

static void breakpoint_panel_destroy(md_tui_panel_t *panel)
{
    free(panel->user_data);
}

static void breakpoint_panel_render(md_tui_panel_t *panel)
{
    WINDOW *win = (WINDOW*)panel->window;
    md_breakpoint_panel_data_t *data = (md_breakpoint_panel_data_t*)panel->user_data;
    if (!data || !win) return;

    int width = panel->rect.width - 2;
    int height = panel->rect.height - 2;

    for (int i = 0; i < height && i < data->count; i++) {
        int y = i + 1;
        md_breakpoint_t *bp = &data->breakpoints[i];

        /* Build display line */
        char line[512];
        char status = bp->enabled ? 'B' : 'b';
        const char *path = bp->path;
        const char *filename = strrchr(path, '/');
        filename = filename ? filename + 1 : path;

        /* Truncate filename to avoid buffer overflow */
        char truncated_filename[64];
        truncate_string(filename, truncated_filename, sizeof(truncated_filename) - 1);

        snprintf(line, sizeof(line), "%c %d: %s:%d",
                 status, bp->id, truncated_filename, bp->line);

        /* Determine color */
        int color = bp->enabled ? MD_TUI_COLOR_BREAKPOINT : MD_TUI_COLOR_DISABLED;
        bool bold = (i == data->selected_index);

        draw_line(win, y, 1, line, width, color, bold);

        /* Draw selection indicator */
        if (i == data->selected_index) {
            mvwaddch(win, y, 1, '>');
        }
    }

    /* Clear remaining lines */
    for (int i = data->count; i < height; i++) {
        wmove(win, i + 1, 1);
        for (int x = 0; x < width; x++) {
            waddch(win, ' ');
        }
    }
}

static bool breakpoint_panel_handle_key(md_tui_panel_t *panel, int key)
{
    md_breakpoint_panel_data_t *data = (md_breakpoint_panel_data_t*)panel->user_data;
    if (!data) return false;

    (void)panel; /* suppress unused warning in non-ncurses mode */

    switch (key) {
        case MD_KEY_UP:
        case 'k':
            if (data->selected_index > 0) {
                data->selected_index--;
            }
            return true;

        case MD_KEY_DOWN:
        case 'j':
            if (data->selected_index < data->count - 1) {
                data->selected_index++;
            }
            return true;

        case MD_KEY_ENTER:
        case ' ':
            /* Toggle breakpoint */
            if (panel->tui && panel->tui->adapter && data->count > 0) {
                md_breakpoint_panel_toggle_selected(panel, panel->tui->adapter);
            }
            return true;

        case 'd':
        case MD_KEY_DELETE:
            /* Delete breakpoint */
            if (panel->tui && panel->tui->adapter && data->count > 0) {
                md_breakpoint_t *bp = md_breakpoint_panel_get_selected(panel);
                if (bp) {
                    md_adapter_remove_breakpoint(panel->tui->adapter, bp->id);
                }
            }
            return true;

        case 'g':
            /* Go to breakpoint location */
            if (data->count > 0 && panel->tui) {
                md_breakpoint_t *bp = &data->breakpoints[data->selected_index];
                md_tui_panel_t *source = md_tui_get_panel(panel->tui, MD_TUI_PANEL_SOURCE);
                if (source) {
                    md_source_panel_load_file(source, bp->path);
                    md_source_panel_scroll_to_line(source, bp->line, true);
                    md_tui_focus_panel(panel->tui, source);
                }
            }
            return true;
    }

    return false;
}

MD_API md_tui_panel_t* md_breakpoint_panel_create(void)
{
    md_tui_panel_t *panel = calloc(1, sizeof(md_tui_panel_t));
    if (!panel) return NULL;

    panel->type = MD_TUI_PANEL_BREAKPOINTS;
    strcpy(panel->title, "Breakpoints");
    panel->vtable = &s_breakpoint_panel_vtable;
    panel->resizable = true;
    panel->visibility = MD_TUI_VISIBLE;

    return panel;
}

MD_API void md_breakpoint_panel_update(md_tui_panel_t *panel,
                                        struct md_adapter *adapter)
{
    md_breakpoint_panel_data_t *data = (md_breakpoint_panel_data_t*)panel->user_data;
    if (!data || !adapter) return;

    /* Get breakpoints from state model - TODO: implement */
    data->count = 0;
}

MD_API md_breakpoint_t* md_breakpoint_panel_get_selected(md_tui_panel_t *panel)
{
    md_breakpoint_panel_data_t *data = (md_breakpoint_panel_data_t*)panel->user_data;
    if (!data || data->selected_index < 0 || data->selected_index >= data->count) {
        return NULL;
    }
    return &data->breakpoints[data->selected_index];
}

MD_API void md_breakpoint_panel_toggle_selected(md_tui_panel_t *panel,
                                                 struct md_adapter *adapter)
{
    md_breakpoint_t *bp = md_breakpoint_panel_get_selected(panel);
    if (bp && adapter) {
        md_adapter_enable_breakpoint(adapter, bp->id, !bp->enabled);
    }
}

/* ============================================================================
 * Call Stack Panel Implementation
 * ============================================================================ */

static md_error_t callstack_panel_init(md_tui_panel_t *panel);
static void callstack_panel_destroy(md_tui_panel_t *panel);
static void callstack_panel_render(md_tui_panel_t *panel);
static bool callstack_panel_handle_key(md_tui_panel_t *panel, int key);

static md_tui_panel_interface_t s_callstack_panel_vtable = {
    .init = callstack_panel_init,
    .destroy = callstack_panel_destroy,
    .render = callstack_panel_render,
    .handle_key = callstack_panel_handle_key,
};

static md_error_t callstack_panel_init(md_tui_panel_t *panel)
{
    md_callstack_panel_data_t *data = calloc(1, sizeof(md_callstack_panel_data_t));
    if (!data) return MD_ERROR_OUT_OF_MEMORY;

    panel->user_data = data;
    return MD_SUCCESS;
}

static void callstack_panel_destroy(md_tui_panel_t *panel)
{
    free(panel->user_data);
}

static void callstack_panel_render(md_tui_panel_t *panel)
{
    WINDOW *win = (WINDOW*)panel->window;
    md_callstack_panel_data_t *data = (md_callstack_panel_data_t*)panel->user_data;
    if (!data || !win) return;

    int width = panel->rect.width - 2;
    int height = panel->rect.height - 2;

    for (int i = 0; i < height && i < data->count; i++) {
        int y = i + 1;
        md_stack_frame_t *frame = &data->frames[i];

        /* Build display line */
        char line[512];
        const char *filename = strrchr(frame->source_path, '/');
        filename = filename ? filename + 1 : frame->source_path;

        /* Truncate filename to avoid buffer overflow */
        char truncated_filename[64];
        truncate_string(filename, truncated_filename, sizeof(truncated_filename) - 1);

        snprintf(line, sizeof(line), "#%d %s() at %s:%d",
                 frame->id, frame->name, truncated_filename, frame->line);

        /* Determine color */
        int color = (i == data->current_frame) ? MD_TUI_COLOR_CURRENT_LINE :
                    MD_TUI_COLOR_NORMAL;
        bool bold = (i == data->selected_index);

        draw_line(win, y, 1, line, width, color, bold);

        /* Draw selection indicator */
        if (i == data->selected_index) {
            mvwaddch(win, y, 1, '>');
        }
    }

    /* Clear remaining lines */
    for (int i = data->count; i < height; i++) {
        wmove(win, i + 1, 1);
        for (int x = 0; x < width; x++) {
            waddch(win, ' ');
        }
    }
}

static bool callstack_panel_handle_key(md_tui_panel_t *panel, int key)
{
    md_callstack_panel_data_t *data = (md_callstack_panel_data_t*)panel->user_data;
    if (!data) return false;

    switch (key) {
        case MD_KEY_UP:
        case 'k':
            if (data->selected_index > 0) {
                data->selected_index--;
            }
            return true;

        case MD_KEY_DOWN:
        case 'j':
            if (data->selected_index < data->count - 1) {
                data->selected_index++;
            }
            return true;

        case MD_KEY_ENTER:
            /* Select frame and show source */
            if (panel->tui && panel->tui->adapter && data->count > 0) {
                md_callstack_panel_select_frame(panel, panel->tui->adapter,
                                                data->selected_index);

                /* Show source at frame location */
                md_tui_panel_t *source = md_tui_get_panel(panel->tui, MD_TUI_PANEL_SOURCE);
                if (source) {
                    md_stack_frame_t *frame = &data->frames[data->selected_index];
                    md_source_panel_load_file(source, frame->source_path);
                    md_source_panel_scroll_to_line(source, frame->line, true);
                    md_source_panel_set_current_line(source, frame->line);
                }
            }
            return true;
    }

    return false;
}

MD_API md_tui_panel_t* md_callstack_panel_create(void)
{
    md_tui_panel_t *panel = calloc(1, sizeof(md_tui_panel_t));
    if (!panel) return NULL;

    panel->type = MD_TUI_PANEL_CALLSTACK;
    strcpy(panel->title, "Call Stack");
    panel->vtable = &s_callstack_panel_vtable;
    panel->resizable = true;
    panel->visibility = MD_TUI_VISIBLE;

    return panel;
}

MD_API void md_callstack_panel_update(md_tui_panel_t *panel,
                                       struct md_adapter *adapter,
                                       int thread_id)
{
    md_callstack_panel_data_t *data = (md_callstack_panel_data_t*)panel->user_data;
    if (!data || !adapter) return;

    data->thread_id = thread_id;
    data->count = 0;
    data->current_frame = 0;

    /* Get stack frames from adapter */
    md_adapter_get_stack_frames(adapter, thread_id, data->frames,
                                 MD_STACK_FRAMES_MAX, &data->count);
}

MD_API md_stack_frame_t* md_callstack_panel_get_selected(md_tui_panel_t *panel)
{
    md_callstack_panel_data_t *data = (md_callstack_panel_data_t*)panel->user_data;
    if (!data || data->selected_index < 0 || data->selected_index >= data->count) {
        return NULL;
    }
    return &data->frames[data->selected_index];
}

MD_API void md_callstack_panel_select_frame(md_tui_panel_t *panel,
                                             struct md_adapter *adapter,
                                             int frame_index)
{
    md_callstack_panel_data_t *data = (md_callstack_panel_data_t*)panel->user_data;
    if (!data || !adapter) return;

    if (frame_index >= 0 && frame_index < data->count) {
        md_adapter_set_active_frame(adapter, data->frames[frame_index].id);
        data->current_frame = frame_index;
    }
}

/* ============================================================================
 * Variable Panel Implementation
 * ============================================================================ */

static md_error_t variable_panel_init(md_tui_panel_t *panel);
static void variable_panel_destroy(md_tui_panel_t *panel);
static void variable_panel_render(md_tui_panel_t *panel);
static bool variable_panel_handle_key(md_tui_panel_t *panel, int key);

static md_tui_panel_interface_t s_variable_panel_vtable = {
    .init = variable_panel_init,
    .destroy = variable_panel_destroy,
    .render = variable_panel_render,
    .handle_key = variable_panel_handle_key,
};

static md_error_t variable_panel_init(md_tui_panel_t *panel)
{
    md_variable_panel_data_t *data = calloc(1, sizeof(md_variable_panel_data_t));
    if (!data) return MD_ERROR_OUT_OF_MEMORY;

    panel->user_data = data;
    return MD_SUCCESS;
}

static void variable_panel_destroy(md_tui_panel_t *panel)
{
    free(panel->user_data);
}

static void variable_panel_render(md_tui_panel_t *panel)
{
    WINDOW *win = (WINDOW*)panel->window;
    md_variable_panel_data_t *data = (md_variable_panel_data_t*)panel->user_data;
    if (!data || !win) return;

    int width = panel->rect.width - 2;
    int height = panel->rect.height - 2;

    for (int i = 0; i < height && i < data->count; i++) {
        int y = i + 1;
        md_var_display_item_t *item = &data->items[i];

        /* Build display line with indentation */
        char line[512];
        char indent[32] = "";
        for (int d = 0; d < item->depth && d < 10; d++) {
            strcat(indent, "  ");
        }

        /* Expand/collapse indicator */
        char expander[4] = " ";
        if (item->has_children) {
            strcpy(expander, item->expanded ? "-" : "+");
        }

        /* Truncate values to avoid buffer overflow */
        char truncated_name[64], truncated_value[64];
        truncate_string(item->var.name, truncated_name, sizeof(truncated_name) - 1);
        truncate_string(item->var.value, truncated_value, sizeof(truncated_value) - 1);

        snprintf(line, sizeof(line), "%s%s %s = %s",
                 indent, expander, truncated_name, truncated_value);

        /* Determine color */
        int color = MD_TUI_COLOR_VARIABLE;
        bool bold = (i == data->selected_index);

        draw_line(win, y, 1, line, width, color, bold);

        /* Draw selection indicator */
        if (i == data->selected_index) {
            mvwaddch(win, y, 1, '>');
        }
    }

    /* Clear remaining lines */
    for (int i = data->count; i < height; i++) {
        wmove(win, i + 1, 1);
        for (int x = 0; x < width; x++) {
            waddch(win, ' ');
        }
    }
}

static bool variable_panel_handle_key(md_tui_panel_t *panel, int key)
{
    md_variable_panel_data_t *data = (md_variable_panel_data_t*)panel->user_data;
    if (!data) return false;

    switch (key) {
        case MD_KEY_UP:
        case 'k':
            if (data->selected_index > 0) {
                data->selected_index--;
            }
            return true;

        case MD_KEY_DOWN:
        case 'j':
            if (data->selected_index < data->count - 1) {
                data->selected_index++;
            }
            return true;

        case MD_KEY_ENTER:
        case ' ':
            /* Expand/collapse */
            if (panel->tui && panel->tui->adapter) {
                md_variable_panel_toggle_expand(panel, panel->tui->adapter,
                                                data->selected_index);
            }
            return true;

        case 'e':
            /* Edit variable value */
            data->editing = true;
            data->editing_index = data->selected_index;
            data->edit_buffer[0] = '\0';
            return true;
    }

    return false;
}

MD_API md_tui_panel_t* md_variable_panel_create(void)
{
    md_tui_panel_t *panel = calloc(1, sizeof(md_tui_panel_t));
    if (!panel) return NULL;

    panel->type = MD_TUI_PANEL_VARIABLES;
    strcpy(panel->title, "Variables");
    panel->vtable = &s_variable_panel_vtable;
    panel->resizable = true;
    panel->visibility = MD_TUI_VISIBLE;

    return panel;
}

MD_API void md_variable_panel_update(md_tui_panel_t *panel,
                                      struct md_adapter *adapter,
                                      int frame_id)
{
    md_variable_panel_data_t *data = (md_variable_panel_data_t*)panel->user_data;
    if (!data || !adapter) return;

    data->frame_id = frame_id;
    data->count = 0;

    /* Get scopes first */
    md_scope_t scopes[10];
    int scope_count = 0;
    md_adapter_get_scopes(adapter, frame_id, scopes, 10, &scope_count);

    /* Get variables for each scope */
    for (int s = 0; s < scope_count && data->count < MD_VARIABLES_MAX; s++) {
        int remaining = MD_VARIABLES_MAX - data->count;
        md_adapter_get_variables(adapter, scopes[s].id,
                                  &data->items[data->count].var,
                                  remaining, &remaining);
        data->count += remaining;
    }
}

MD_API void md_variable_panel_toggle_expand(md_tui_panel_t *panel,
                                             struct md_adapter *adapter,
                                             int index)
{
    md_variable_panel_data_t *data = (md_variable_panel_data_t*)panel->user_data;
    if (!data || !adapter || index < 0 || index >= data->count) return;

    md_var_display_item_t *item = &data->items[index];

    if (!item->has_children) return;

    item->expanded = !item->expanded;

    /* If expanding and children not loaded, load them */
    if (item->expanded && !item->children_loaded) {
        /* This would load child variables */
        item->children_loaded = true;
    }
}

MD_API md_error_t md_variable_panel_set_value(md_tui_panel_t *panel,
                                               struct md_adapter *adapter,
                                               int index,
                                               const char *value)
{
    md_variable_panel_data_t *data = (md_variable_panel_data_t*)panel->user_data;
    if (!data || !adapter || index < 0 || index >= data->count) {
        return MD_ERROR_INVALID_ARG;
    }

    return md_adapter_set_variable(adapter, data->items[index].var.variables_reference,
                                   data->items[index].var.name, value);
}

/* ============================================================================
 * Output Panel Implementation
 * ============================================================================ */

static md_error_t output_panel_init(md_tui_panel_t *panel);
static void output_panel_destroy(md_tui_panel_t *panel);
static void output_panel_render(md_tui_panel_t *panel);
static bool output_panel_handle_key(md_tui_panel_t *panel, int key);

static md_tui_panel_interface_t s_output_panel_vtable = {
    .init = output_panel_init,
    .destroy = output_panel_destroy,
    .render = output_panel_render,
    .handle_key = output_panel_handle_key,
};

static md_error_t output_panel_init(md_tui_panel_t *panel)
{
    md_output_panel_data_t *data = calloc(1, sizeof(md_output_panel_data_t));
    if (!data) return MD_ERROR_OUT_OF_MEMORY;

    data->auto_scroll = true;
    panel->user_data = data;
    return MD_SUCCESS;
}

static void output_panel_destroy(md_tui_panel_t *panel)
{
    md_output_panel_data_t *data = (md_output_panel_data_t*)panel->user_data;
    if (data) {
        for (int i = 0; i < data->count; i++) {
            free(data->lines[i].text);
        }
        free(data);
    }
}

static void output_panel_render(md_tui_panel_t *panel)
{
    WINDOW *win = (WINDOW*)panel->window;
    md_output_panel_data_t *data = (md_output_panel_data_t*)panel->user_data;
    if (!data || !win) return;

    int width = panel->rect.width - 2;
    int height = panel->rect.height - 2;

    int start = data->count - height - data->scroll_offset;
    if (start < 0) start = 0;

    for (int i = 0; i < height; i++) {
        int y = i + 1;
        int line_idx = start + i;

        if (line_idx < data->count) {
            md_output_line_t *line = &data->lines[line_idx];

            /* Determine color based on type */
            int color = MD_TUI_COLOR_NORMAL;
            switch (line->type) {
                case MD_OUTPUT_STDOUT:  color = MD_TUI_COLOR_NORMAL; break;
                case MD_OUTPUT_STDERR:  color = MD_TUI_COLOR_ERROR; break;
                case MD_OUTPUT_LOG:     color = MD_TUI_COLOR_COMMENT; break;
                case MD_OUTPUT_COMMAND: color = MD_TUI_COLOR_KEYWORD; break;
                case MD_OUTPUT_RESULT:  color = MD_TUI_COLOR_SUCCESS; break;
                case MD_OUTPUT_ERROR:   color = MD_TUI_COLOR_ERROR; break;
            }

            draw_line(win, y, 1, line->text, width, color, false);
        } else {
            wmove(win, y, 1);
            for (int x = 0; x < width; x++) {
                waddch(win, ' ');
            }
        }
    }
}

static bool output_panel_handle_key(md_tui_panel_t *panel, int key)
{
    md_output_panel_data_t *data = (md_output_panel_data_t*)panel->user_data;
    if (!data) return false;

    int height = panel->rect.height - 2;

    switch (key) {
        case MD_KEY_UP:
        case 'k':
            if (data->scroll_offset < data->count - height) {
                data->scroll_offset++;
            }
            data->auto_scroll = false;
            return true;

        case MD_KEY_DOWN:
        case 'j':
            if (data->scroll_offset > 0) {
                data->scroll_offset--;
            }
            return true;

        case MD_KEY_PAGE_UP:
            data->scroll_offset += height;
            if (data->scroll_offset > data->count - height) {
                data->scroll_offset = data->count - height;
                if (data->scroll_offset < 0) data->scroll_offset = 0;
            }
            data->auto_scroll = false;
            return true;

        case MD_KEY_PAGE_DOWN:
            data->scroll_offset -= height;
            if (data->scroll_offset < 0) data->scroll_offset = 0;
            return true;

        case 'G':
            /* Go to end (enable auto-scroll) */
            data->scroll_offset = 0;
            data->auto_scroll = true;
            return true;
    }

    return false;
}

MD_API md_tui_panel_t* md_output_panel_create(void)
{
    md_tui_panel_t *panel = calloc(1, sizeof(md_tui_panel_t));
    if (!panel) return NULL;

    panel->type = MD_TUI_PANEL_OUTPUT;
    strcpy(panel->title, "Output");
    panel->vtable = &s_output_panel_vtable;
    panel->resizable = true;
    panel->visibility = MD_TUI_VISIBLE;

    return panel;
}

MD_API void md_output_panel_add_line(md_tui_panel_t *panel,
                                      md_output_line_type_t type,
                                      const char *format, ...)
{
    md_output_panel_data_t *data = (md_output_panel_data_t*)panel->user_data;
    if (!data || !format) return;

    /* Format message */
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    /* Add to circular buffer */
    int idx;
    if (data->count < MD_OUTPUT_LINES_MAX) {
        idx = data->count++;
    } else {
        /* Free oldest line */
        free(data->lines[data->start_index].text);
        idx = data->start_index;
        data->start_index = (data->start_index + 1) % MD_OUTPUT_LINES_MAX;
    }

    data->lines[idx].text = md_strdup(buffer);
    data->lines[idx].type = type;
    data->lines[idx].timestamp = (uint64_t)time(NULL) * 1000;
}

MD_API void md_output_panel_clear(md_tui_panel_t *panel)
{
    md_output_panel_data_t *data = (md_output_panel_data_t*)panel->user_data;
    if (!data) return;

    for (int i = 0; i < data->count; i++) {
        free(data->lines[i].text);
    }
    data->count = 0;
    data->start_index = 0;
    data->scroll_offset = 0;
}

/* ============================================================================
 * Thread Panel Implementation
 * ============================================================================ */

static md_error_t thread_panel_init(md_tui_panel_t *panel);
static void thread_panel_destroy(md_tui_panel_t *panel);
static void thread_panel_render(md_tui_panel_t *panel);
static bool thread_panel_handle_key(md_tui_panel_t *panel, int key);

static md_tui_panel_interface_t s_thread_panel_vtable = {
    .init = thread_panel_init,
    .destroy = thread_panel_destroy,
    .render = thread_panel_render,
    .handle_key = thread_panel_handle_key,
};

static md_error_t thread_panel_init(md_tui_panel_t *panel)
{
    md_thread_panel_data_t *data = calloc(1, sizeof(md_thread_panel_data_t));
    if (!data) return MD_ERROR_OUT_OF_MEMORY;

    panel->user_data = data;
    return MD_SUCCESS;
}

static void thread_panel_destroy(md_tui_panel_t *panel)
{
    free(panel->user_data);
}

static void thread_panel_render(md_tui_panel_t *panel)
{
    WINDOW *win = (WINDOW*)panel->window;
    md_thread_panel_data_t *data = (md_thread_panel_data_t*)panel->user_data;
    if (!data || !win) return;

    int width = panel->rect.width - 2;
    int height = panel->rect.height - 2;

    for (int i = 0; i < height && i < data->count; i++) {
        int y = i + 1;
        md_thread_t *thread = &data->threads[i];

        /* Build display line */
        char line[512];
        const char *state_str = "running";
        switch (thread->state) {
            case MD_THREAD_STATE_STOPPED: state_str = "stopped"; break;
            case MD_THREAD_STATE_EXITED:  state_str = "exited"; break;
            default: break;
        }

        snprintf(line, sizeof(line), "[%d] %s (%s)",
                 thread->id, thread->name, state_str);

        /* Determine color */
        int color = md_tui_get_thread_color(panel->tui, thread->state);
        bool bold = (i == data->selected_index);

        draw_line(win, y, 1, line, width, color, bold);

        /* Current thread marker */
        if (thread->id == data->current_thread) {
            mvwaddch(win, y, 1, '*');
        }
    }

    /* Clear remaining lines */
    for (int i = data->count; i < height; i++) {
        wmove(win, i + 1, 1);
        for (int x = 0; x < width; x++) {
            waddch(win, ' ');
        }
    }
}

static bool thread_panel_handle_key(md_tui_panel_t *panel, int key)
{
    md_thread_panel_data_t *data = (md_thread_panel_data_t*)panel->user_data;
    if (!data) return false;

    switch (key) {
        case MD_KEY_UP:
        case 'k':
            if (data->selected_index > 0) {
                data->selected_index--;
            }
            return true;

        case MD_KEY_DOWN:
        case 'j':
            if (data->selected_index < data->count - 1) {
                data->selected_index++;
            }
            return true;

        case MD_KEY_ENTER:
            /* Select thread */
            if (panel->tui && panel->tui->adapter && data->count > 0) {
                md_thread_t *thread = &data->threads[data->selected_index];
                md_thread_panel_select_thread(panel, panel->tui->adapter, thread->id);
            }
            return true;
    }

    return false;
}

MD_API md_tui_panel_t* md_thread_panel_create(void)
{
    md_tui_panel_t *panel = calloc(1, sizeof(md_tui_panel_t));
    if (!panel) return NULL;

    panel->type = MD_TUI_PANEL_THREADS;
    strcpy(panel->title, "Threads");
    panel->vtable = &s_thread_panel_vtable;
    panel->resizable = true;
    panel->visibility = MD_TUI_VISIBLE;

    return panel;
}

MD_API void md_thread_panel_update(md_tui_panel_t *panel,
                                    struct md_adapter *adapter)
{
    md_thread_panel_data_t *data = (md_thread_panel_data_t*)panel->user_data;
    if (!data || !adapter) return;

    data->count = 0;
    data->current_thread = md_adapter_get_current_thread_id(adapter);

    md_adapter_get_threads(adapter, data->threads, MD_THREADS_MAX, &data->count);
}

MD_API void md_thread_panel_select_thread(md_tui_panel_t *panel,
                                           struct md_adapter *adapter,
                                           int thread_id)
{
    md_thread_panel_data_t *data = (md_thread_panel_data_t*)panel->user_data;
    if (!data || !adapter) return;

    md_adapter_set_active_thread(adapter, thread_id);
    data->current_thread = thread_id;

    /* Update related panels */
    if (panel->tui) {
        md_tui_panel_t *callstack = md_tui_get_panel(panel->tui, MD_TUI_PANEL_CALLSTACK);
        if (callstack) {
            md_callstack_panel_update(callstack, adapter, thread_id);
        }
    }
}

/* ============================================================================
 * Status Panel Implementation
 * ============================================================================ */

static md_error_t status_panel_init(md_tui_panel_t *panel);
static void status_panel_render(md_tui_panel_t *panel);

static md_tui_panel_interface_t s_status_panel_vtable = {
    .init = status_panel_init,
    .render = status_panel_render,
};

static md_error_t status_panel_init(md_tui_panel_t *panel)
{
    md_status_panel_data_t *data = calloc(1, sizeof(md_status_panel_data_t));
    if (!data) return MD_ERROR_OUT_OF_MEMORY;

    panel->user_data = data;
    return MD_SUCCESS;
}

static void status_panel_render(md_tui_panel_t *panel)
{
    WINDOW *win = (WINDOW*)panel->window;
    md_status_panel_data_t *data = (md_status_panel_data_t*)panel->user_data;
    if (!data || !win) return;

    int width = panel->rect.width;

    /* Build status string */
    char status[512];
    int pos = 0;

    /* Program name */
    if (data->program_name[0]) {
        pos += snprintf(status + pos, sizeof(status) - pos, "[%s] ",
                        data->program_name);
    }

    /* State */
    pos += snprintf(status + pos, sizeof(status) - pos, "%s",
                    md_adapter_state_string(data->state));

    /* File and line */
    if (data->file_name[0]) {
        pos += snprintf(status + pos, sizeof(status) - pos, " | %s:%d",
                        data->file_name, data->line_number);
    }

    /* Function */
    if (data->function_name[0]) {
        pos += snprintf(status + pos, sizeof(status) - pos, " in %s",
                        data->function_name);
    }

    /* Draw status bar */
    wattron(win, COLOR_PAIR(MD_TUI_COLOR_STATUS_BAR) | A_BOLD);
    mvwprintw(win, 0, 0, "%-*s", width, status);
    wattroff(win, COLOR_PAIR(MD_TUI_COLOR_STATUS_BAR) | A_BOLD);
}

MD_API md_tui_panel_t* md_status_panel_create(void)
{
    md_tui_panel_t *panel = calloc(1, sizeof(md_tui_panel_t));
    if (!panel) return NULL;

    panel->type = MD_TUI_PANEL_STATUS;
    panel->vtable = &s_status_panel_vtable;
    panel->resizable = false;
    panel->visibility = MD_TUI_VISIBLE;

    return panel;
}

MD_API void md_status_panel_update(md_tui_panel_t *panel,
                                    struct md_adapter *adapter)
{
    md_status_panel_data_t *data = (md_status_panel_data_t*)panel->user_data;
    if (!data || !adapter) return;

    /* Get state */
    data->state = md_adapter_get_state(adapter);

    /* Get debug state */
    md_debug_state_t state;
    if (md_adapter_get_debug_state(adapter, &state) == MD_SUCCESS) {
        strncpy(data->program_name, state.program_path,
                sizeof(data->program_name) - 1);
        /* TODO: add current file/line/function to md_debug_state_t */
        data->file_name[0] = '\0';
        data->line_number = 0;
        data->function_name[0] = '\0';
        data->thread_count = 0;
        data->breakpoint_count = 0;
    }
}

/* ============================================================================
 * Help Panel Implementation
 * ============================================================================ */

static const md_help_entry_t s_help_entries[] = {
    /* Navigation */
    {"h/j/k/l", "Movement", "Move left/down/up/right"},
    {"H/L", "Panels", "Previous/next panel"},
    {"Ctrl-B/F", "Scroll", "Page up/down"},
    {"g/G", "Goto", "Go to start/end"},

    /* Debugging */
    {"F5/c", "Continue", "Continue execution"},
    {"F6/n", "Step Over", "Step over current line"},
    {"F7/s", "Step Into", "Step into function"},
    {"F8/f", "Step Out", "Step out of function"},
    {"r", "Restart", "Restart debuggee"},
    {"Q", "Quit", "Quit debugger"},

    /* Breakpoints */
    {"F9/b", "Breakpoint", "Toggle breakpoint"},
    {"d", "Delete", "Delete breakpoint"},

    /* Other */
    {":", "Command", "Enter command mode"},
    {"?", "Help", "Show this help"},
    {"R", "Redraw", "Redraw screen"},

    {NULL, NULL, NULL}  /* Sentinel */
};

static md_error_t help_panel_init(md_tui_panel_t *panel);
static void help_panel_render(md_tui_panel_t *panel);
static bool help_panel_handle_key(md_tui_panel_t *panel, int key);

static md_tui_panel_interface_t s_help_panel_vtable = {
    .init = help_panel_init,
    .render = help_panel_render,
    .handle_key = help_panel_handle_key,
};

static md_error_t help_panel_init(md_tui_panel_t *panel)
{
    md_help_panel_data_t *data = calloc(1, sizeof(md_help_panel_data_t));
    if (!data) return MD_ERROR_OUT_OF_MEMORY;

    data->entries = s_help_entries;
    for (data->count = 0; s_help_entries[data->count].key; data->count++);

    panel->user_data = data;
    return MD_SUCCESS;
}

static void help_panel_render(md_tui_panel_t *panel)
{
    WINDOW *win = (WINDOW*)panel->window;
    md_help_panel_data_t *data = (md_help_panel_data_t*)panel->user_data;
    if (!data || !win) return;

    (void)panel; /* suppress unused warning in non-ncurses mode */
    int height = panel->rect.height - 2;

    /* Title */
    wattron(win, A_BOLD | COLOR_PAIR(MD_TUI_COLOR_TITLE));
    mvwprintw(win, 1, 2, "Magic Debugger - Key Bindings");
    wattroff(win, A_BOLD | COLOR_PAIR(MD_TUI_COLOR_TITLE));

    int y = 3;
    for (int i = data->scroll_offset; i < data->count && y < height; i++) {
        const md_help_entry_t *entry = &data->entries[i];

        /* Key */
        wattron(win, A_BOLD | COLOR_PAIR(MD_TUI_COLOR_KEYWORD));
        mvwprintw(win, y, 2, "%-12s", entry->key);
        wattroff(win, A_BOLD | COLOR_PAIR(MD_TUI_COLOR_KEYWORD));

        /* Command and description */
        wattron(win, COLOR_PAIR(MD_TUI_COLOR_NORMAL));
        mvwprintw(win, y, 15, "%-15s %s", entry->command, entry->description);
        wattroff(win, COLOR_PAIR(MD_TUI_COLOR_NORMAL));

        y++;
    }
}

static bool help_panel_handle_key(md_tui_panel_t *panel, int key)
{
    md_help_panel_data_t *data = (md_help_panel_data_t*)panel->user_data;
    if (!data) return false;

    int height = panel->rect.height - 2;

    switch (key) {
        case MD_KEY_UP:
        case 'k':
            if (data->scroll_offset > 0) {
                data->scroll_offset--;
            }
            return true;

        case MD_KEY_DOWN:
        case 'j':
            if (data->scroll_offset < data->count - height + 3) {
                data->scroll_offset++;
            }
            return true;

        case 'q':
        case MD_KEY_ESCAPE:
        case '?':
            /* Close help */
            if (panel->tui) {
                md_tui_set_panel_visible(panel->tui, MD_TUI_PANEL_HELP, false);
                md_tui_focus_panel(panel->tui, panel->tui->source_panel);
            }
            return true;
    }

    return false;
}

MD_API md_tui_panel_t* md_help_panel_create(void)
{
    md_tui_panel_t *panel = calloc(1, sizeof(md_tui_panel_t));
    if (!panel) return NULL;

    panel->type = MD_TUI_PANEL_HELP;
    strcpy(panel->title, "Help");
    panel->vtable = &s_help_panel_vtable;
    panel->visibility = MD_TUI_HIDDEN;  /* Hidden by default */

    return panel;
}

/* ============================================================================
 * Panel Factory
 * ============================================================================ */

MD_API md_tui_panel_t* md_tui_panel_create_by_type(md_tui_panel_type_t type)
{
    switch (type) {
        case MD_TUI_PANEL_SOURCE:      return md_source_panel_create();
        case MD_TUI_PANEL_BREAKPOINTS: return md_breakpoint_panel_create();
        case MD_TUI_PANEL_CALLSTACK:   return md_callstack_panel_create();
        case MD_TUI_PANEL_VARIABLES:   return md_variable_panel_create();
        case MD_TUI_PANEL_THREADS:     return md_thread_panel_create();
        case MD_TUI_PANEL_OUTPUT:      return md_output_panel_create();
        case MD_TUI_PANEL_STATUS:      return md_status_panel_create();
        case MD_TUI_PANEL_HELP:        return md_help_panel_create();
        default: return NULL;
    }
}

MD_API md_error_t md_tui_create_standard_panels(md_tui_t *tui)
{
    if (!tui) return MD_ERROR_NULL_POINTER;

    /* Create source panel */
    md_tui_panel_t *source = md_source_panel_create();
    if (source) {
        md_tui_add_panel(tui, source);
    }

    /* Create breakpoints panel */
    md_tui_panel_t *breakpoints = md_breakpoint_panel_create();
    if (breakpoints) {
        md_tui_add_panel(tui, breakpoints);
    }

    /* Create callstack panel */
    md_tui_panel_t *callstack = md_callstack_panel_create();
    if (callstack) {
        md_tui_add_panel(tui, callstack);
    }

    /* Create variables panel */
    md_tui_panel_t *variables = md_variable_panel_create();
    if (variables) {
        md_tui_add_panel(tui, variables);
    }

    /* Create output panel */
    md_tui_panel_t *output = md_output_panel_create();
    if (output) {
        md_tui_add_panel(tui, output);
    }

    /* Create status bar */
    md_tui_panel_t *status = md_status_panel_create();
    if (status) {
        md_tui_add_panel(tui, status);
    }

    /* Create help panel (hidden by default) */
    md_tui_panel_t *help = md_help_panel_create();
    if (help) {
        md_tui_add_panel(tui, help);
    }

    /* Update layout */
    md_tui_layout_update(tui);

    return MD_SUCCESS;
}
