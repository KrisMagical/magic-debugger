/**
 * @file tui.c
 * @brief Terminal User Interface Implementation
 *
 * This file implements the TUI subsystem for Magic Debugger using ncurses.
 * Provides an interactive debugging interface with multiple panels,
 * keyboard navigation, and real-time state updates.
 *
 * Phase 5: Frontend - Terminal User Interface
 */

#include "tui.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <locale.h>
#include <time.h>
#include <unistd.h>  /* for isatty, STDIN_FILENO */

/* ncurses headers - optional for compilation */
#ifdef HAVE_NCURSES
#include <ncurses.h>
#include <panel.h>
#else
/* Stub definitions for building without ncurses */
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
#define COLOR_DEFAULT -1
#define A_BOLD      0x01000000
#define A_NORMAL    0
#define ACS_RARROW  '>'
#define ACS_LARROW  '<'
#define ACS_CKBOARD '#'
#define BUTTON1_CLICKED    0x00000001
#define BUTTON_CTRL        0x01000000
#define BUTTON_ALT         0x02000000
#define BUTTON_SHIFT       0x04000000
#define ALL_MOUSE_EVENTS   0x0FFFFFFF
#define REPORT_MOUSE_POSITION 0x10000000
#define COLOR_WHITE   7
#define COLOR_BLACK   0
#define COLOR_RED     1
#define COLOR_GREEN   2
#define COLOR_YELLOW  3
#define COLOR_BLUE    4
#define COLOR_MAGENTA 5
#define COLOR_CYAN    6
#define COLOR_PAIRS   256
static inline int has_colors(void) { return 0; }
static inline int start_color(void) { return 0; }
static inline int use_default_colors(void) { return 0; }
static inline int init_pair(int p, int f, int b) { (void)p; (void)f; (void)b; return 0; }
static inline int COLOR_PAIR(int p) { return p; }
static inline int initscr(void) { return 0; }
static inline int endwin(void) { return 0; }
static inline int cbreak(void) { return 0; }
static inline int noecho(void) { return 0; }
static inline int keypad(void *w, int bf) { (void)w; (void)bf; return 0; }
static inline int timeout(int t) { (void)t; return 0; }
static inline int curs_set(int vis) { (void)vis; return 0; }
static inline int getch(void) { return ERR; }
static inline WINDOW* newwin(int h, int w, int y, int x) { (void)h; (void)w; (void)y; (void)x; return NULL; }
static inline int delwin(WINDOW *w) { (void)w; return 0; }
static inline int wmove(WINDOW *w, int y, int x) { (void)w; (void)y; (void)x; return 0; }
static inline int waddch(WINDOW *w, int c) { (void)w; (void)c; return 0; }
static inline int wprintw(WINDOW *w, const char *fmt, ...) { (void)w; (void)fmt; return 0; }
static inline int mvwprintw(WINDOW *w, int y, int x, const char *fmt, ...) { (void)w; (void)y; (void)x; (void)fmt; return 0; }
static inline int mvwaddch(WINDOW *w, int y, int x, int c) { (void)w; (void)y; (void)x; (void)c; return 0; }
static inline int wattron(WINDOW *w, int a) { (void)w; (void)a; return 0; }
static inline int wattroff(WINDOW *w, int a) { (void)w; (void)a; return 0; }
static inline int wclear(WINDOW *w) { (void)w; return 0; }
static inline int werase(WINDOW *w) { (void)w; return 0; }
static inline int box(WINDOW *w, int v, int h) { (void)w; (void)v; (void)h; return 0; }
static inline int wresize(WINDOW *w, int h, int w_) { (void)w; (void)h; (void)w_; return 0; }
static inline int mvwin(WINDOW *w, int y, int x) { (void)w; (void)y; (void)x; return 0; }
static inline int scrollok(WINDOW *w, int bf) { (void)w; (void)bf; return 0; }
static inline int getmaxyx(WINDOW *w, int y, int x) { (void)w; (void)y; (void)x; return 0; }
static inline PANEL* new_panel(WINDOW *w) { (void)w; return NULL; }
static inline int del_panel(PANEL *p) { (void)p; return 0; }
static inline int show_panel(PANEL *p) { (void)p; return 0; }
static inline int hide_panel(PANEL *p) { (void)p; return 0; }
static inline int top_panel(PANEL *p) { (void)p; return 0; }
static inline int update_panels(void) { return 0; }
static inline int doupdate(void) { return 0; }
static inline int mousemask(int m, void *old) { (void)m; (void)old; return 0; }
static inline int mouseinterval(int t) { (void)t; return 0; }
static inline int getmouse(void *m) { (void)m; return ERR; }
static inline int nodelay(void *w, int bf) { (void)w; (void)bf; return 0; }
static inline int clrtoeol(void) { return 0; }
static inline int wnoutrefresh(WINDOW *w) { (void)w; return 0; }
static inline int clearok(WINDOW *w, int bf) { (void)w; (void)bf; return 0; }
static inline int intrflush(void *w, int bf) { (void)w; (void)bf; return 0; }
static int COLS = 80;
static int LINES = 24;
#define getmaxyx(w, h, w_) do { h = LINES; w_ = COLS; } while(0)
/* stdscr stub */
static WINDOW *stdscr = NULL;
/* MEVENT stub */
typedef struct {
    int x, y;
    int bstate;
} MEVENT;
static inline int attron(int a) { (void)a; return 0; }
static inline int attroff(int a) { (void)a; return 0; }
static inline int mvprintw(int y, int x, const char *fmt, ...) { (void)y; (void)x; (void)fmt; return 0; }
#define MD_UNUSED_FUNCTION __attribute__((unused))
#endif

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

/** Convert milliseconds to microseconds for select() */
#define MS_TO_US(ms) ((ms) * 1000)

/** Default timeout for getch() in milliseconds */
#define GETCH_TIMEOUT_MS 50

/** Message display duration in milliseconds */
#define MESSAGE_DURATION_MS 3000

/* ============================================================================
 * Internal Panel Data
 * ============================================================================ */

/**
 * @brief Internal panel data for ncurses
 */
typedef struct md_tui_panel_data {
    WINDOW *win;            /**< ncurses window */
    PANEL *panel;           /**< ncurses panel */
    bool needs_redraw;      /**< Redraw flag */
} md_tui_panel_data_t;

/* ============================================================================
 * Static Variables
 * ============================================================================ */

/** Panel data storage */
static md_tui_panel_data_t s_panel_data[MD_TUI_MAX_PANELS];
static int s_panel_data_count = 0;

/** Global TUI instance (for signal handling) */
static md_tui_t *s_current_tui = NULL;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

MD_API md_error_t md_tui_process_events(md_tui_t *tui);

/* ============================================================================
 * Command Name Table
 * ============================================================================ */

static const struct {
    md_tui_command_t cmd;
    const char *name;
    const char *description;
} s_command_table[] = {
    /* Navigation */
    {MD_TUI_CMD_UP,           "up",        "Move up"},
    {MD_TUI_CMD_DOWN,         "down",      "Move down"},
    {MD_TUI_CMD_LEFT,         "left",      "Move left"},
    {MD_TUI_CMD_RIGHT,        "right",     "Move right"},
    {MD_TUI_CMD_PAGE_UP,      "pageup",    "Page up"},
    {MD_TUI_CMD_PAGE_DOWN,    "pagedown",  "Page down"},
    {MD_TUI_CMD_HOME,         "home",      "Go to start"},
    {MD_TUI_CMD_END,          "end",       "Go to end"},
    {MD_TUI_CMD_GOTO_LINE,    "goto",      "Go to line"},

    /* Panel Management */
    {MD_TUI_CMD_NEXT_PANEL,     "nextpanel",     "Next panel"},
    {MD_TUI_CMD_PREV_PANEL,     "prevpanel",     "Previous panel"},
    {MD_TUI_CMD_FOCUS_SOURCE,   "source",        "Focus source panel"},
    {MD_TUI_CMD_FOCUS_BREAKPOINTS, "breakpoints", "Focus breakpoints panel"},
    {MD_TUI_CMD_FOCUS_CALLSTACK,  "callstack",   "Focus call stack panel"},
    {MD_TUI_CMD_FOCUS_VARIABLES,  "variables",   "Focus variables panel"},
    {MD_TUI_CMD_FOCUS_OUTPUT,    "output",       "Focus output panel"},
    {MD_TUI_CMD_TOGGLE_PANEL,    "togglepanel",  "Toggle panel visibility"},
    {MD_TUI_CMD_ZOOM_PANEL,      "zoom",         "Zoom current panel"},

    /* Debugging Control */
    {MD_TUI_CMD_CONTINUE,     "continue",  "Continue execution"},
    {MD_TUI_CMD_STEP_OVER,    "next",      "Step over"},
    {MD_TUI_CMD_STEP_INTO,    "step",      "Step into"},
    {MD_TUI_CMD_STEP_OUT,     "finish",    "Step out"},
    {MD_TUI_CMD_PAUSE,        "pause",     "Pause execution"},
    {MD_TUI_CMD_RESTART,      "restart",   "Restart debuggee"},
    {MD_TUI_CMD_STOP,         "stop",      "Stop debugging"},
    {MD_TUI_CMD_RUN_TO_CURSOR, "tobreak",  "Run to cursor"},

    /* Breakpoints */
    {MD_TUI_CMD_TOGGLE_BREAKPOINT,  "break",   "Toggle breakpoint"},
    {MD_TUI_CMD_ADD_BREAKPOINT,     "addbreak","Add breakpoint"},
    {MD_TUI_CMD_REMOVE_BREAKPOINT,  "delbreak","Remove breakpoint"},
    {MD_TUI_CMD_ENABLE_BREAKPOINT,  "enable",  "Enable breakpoint"},
    {MD_TUI_CMD_DISABLE_BREAKPOINT, "disable", "Disable breakpoint"},
    {MD_TUI_CMD_EDIT_BREAKPOINT,    "editbreak","Edit breakpoint"},

    /* Variables */
    {MD_TUI_CMD_EVALUATE,      "eval",     "Evaluate expression"},
    {MD_TUI_CMD_WATCH_ADD,     "watch",    "Add watch expression"},
    {MD_TUI_CMD_WATCH_REMOVE,  "unwatch",  "Remove watch"},
    {MD_TUI_CMD_SET_VARIABLE,  "set",      "Set variable"},
    {MD_TUI_CMD_PRINT_VARIABLE,"print",    "Print variable"},

    /* Stack */
    {MD_TUI_CMD_UP_FRAME,      "up",       "Go up stack frame"},
    {MD_TUI_CMD_DOWN_FRAME,    "down",     "Go down stack frame"},
    {MD_TUI_CMD_SELECT_THREAD, "thread",   "Select thread"},

    /* Source */
    {MD_TUI_CMD_OPEN_FILE,     "open",     "Open file"},
    {MD_TUI_CMD_FIND,          "find",     "Find in file"},
    {MD_TUI_CMD_FIND_NEXT,     "findnext", "Find next"},
    {MD_TUI_CMD_FIND_PREV,     "findprev", "Find previous"},
    {MD_TUI_CMD_GOTO_PC,       "pc",       "Go to program counter"},

    /* View */
    {MD_TUI_CMD_TOGGLE_ASM,      "asm",      "Toggle disassembly"},
    {MD_TUI_CMD_TOGGLE_REGISTERS,"regs",     "Toggle registers"},
    {MD_TUI_CMD_TOGGLE_MEMORY,   "memory",   "Toggle memory view"},
    {MD_TUI_CMD_REFRESH,         "refresh",  "Refresh display"},

    /* General */
    {MD_TUI_CMD_COMMAND_MODE, "command",   "Enter command mode"},
    {MD_TUI_CMD_HELP,         "help",      "Show help"},
    {MD_TUI_CMD_QUIT,         "quit",      "Quit debugger"},
    {MD_TUI_CMD_FORCE_QUIT,   "quit!",     "Force quit"},
    {MD_TUI_CMD_REDRAW,       "redraw",    "Redraw screen"},

    {MD_TUI_CMD_NONE, NULL, NULL}  /* Sentinel */
};

/* ============================================================================
 * Panel Type Names
 * ============================================================================ */

static const char *s_panel_type_names[] = {
    [MD_TUI_PANEL_SOURCE]      = "Source",
    [MD_TUI_PANEL_BREAKPOINTS] = "Breakpoints",
    [MD_TUI_PANEL_CALLSTACK]   = "Call Stack",
    [MD_TUI_PANEL_VARIABLES]   = "Variables",
    [MD_TUI_PANEL_THREADS]     = "Threads",
    [MD_TUI_PANEL_OUTPUT]      = "Output",
    [MD_TUI_PANEL_WATCH]       = "Watch",
    [MD_TUI_PANEL_DISASM]      = "Disassembly",
    [MD_TUI_PANEL_REGISTERS]   = "Registers",
    [MD_TUI_PANEL_MEMORY]      = "Memory",
    [MD_TUI_PANEL_COMMAND]     = "Command",
    [MD_TUI_PANEL_HELP]        = "Help",
    [MD_TUI_PANEL_STATUS]      = "Status",
};

/* ============================================================================
 * Internal Functions - Color Management
 * ============================================================================ */

/**
 * @brief Initialize color pairs
 */
static void init_color_pairs(void)
{
    if (!has_colors()) {
        return;
    }

    start_color();
    use_default_colors();

    /* Define color pairs: foreground, background */
    init_pair(MD_TUI_COLOR_NORMAL,       COLOR_WHITE,   COLOR_DEFAULT);
    init_pair(MD_TUI_COLOR_HIGHLIGHT,    COLOR_YELLOW,  COLOR_DEFAULT);
    init_pair(MD_TUI_COLOR_BREAKPOINT,   COLOR_RED,     COLOR_DEFAULT);
    init_pair(MD_TUI_COLOR_CURRENT_LINE, COLOR_BLACK,   COLOR_CYAN);
    init_pair(MD_TUI_COLOR_STATUS_BAR,   COLOR_BLACK,   COLOR_WHITE);
    init_pair(MD_TUI_COLOR_ERROR,        COLOR_RED,     COLOR_DEFAULT);
    init_pair(MD_TUI_COLOR_WARNING,      COLOR_YELLOW,  COLOR_DEFAULT);
    init_pair(MD_TUI_COLOR_SUCCESS,      COLOR_GREEN,   COLOR_DEFAULT);
    init_pair(MD_TUI_COLOR_KEYWORD,      COLOR_MAGENTA, COLOR_DEFAULT);
    init_pair(MD_TUI_COLOR_STRING,       COLOR_GREEN,   COLOR_DEFAULT);
    init_pair(MD_TUI_COLOR_COMMENT,      COLOR_CYAN,    COLOR_DEFAULT);
    init_pair(MD_TUI_COLOR_NUMBER,       COLOR_YELLOW,  COLOR_DEFAULT);
    init_pair(MD_TUI_COLOR_TYPE,         COLOR_CYAN,    COLOR_DEFAULT);
    init_pair(MD_TUI_COLOR_FUNCTION,     COLOR_BLUE,    COLOR_DEFAULT);
    init_pair(MD_TUI_COLOR_VARIABLE,     COLOR_WHITE,   COLOR_DEFAULT);
    init_pair(MD_TUI_COLOR_DISABLED,     COLOR_BLACK,   COLOR_DEFAULT);
    init_pair(MD_TUI_COLOR_SELECTED,     COLOR_BLACK,   COLOR_GREEN);
    init_pair(MD_TUI_COLOR_BORDER,       COLOR_CYAN,    COLOR_DEFAULT);
    init_pair(MD_TUI_COLOR_TITLE,        COLOR_CYAN,    COLOR_DEFAULT);
}

/* ============================================================================
 * Internal Functions - Panel Helpers
 * ============================================================================ */

/**
 * @brief Create ncurses window for panel
 */
static WINDOW* create_panel_window(md_tui_panel_t *panel)
{
    md_tui_rect_t *rect = &panel->rect;
    WINDOW *win = newwin(rect->height, rect->width, rect->y, rect->x);
    if (!win) {
        return NULL;
    }

    /* Enable keypad for function keys */
    keypad(win, TRUE);

    /* Enable scrolling */
    scrollok(win, FALSE);

    return win;
}

/**
 * @brief Draw panel border and title
 */
static void draw_panel_border(md_tui_panel_t *panel)
{
    WINDOW *win = (WINDOW*)panel->window;
    if (!win) return;

    /* Set border color */
    wattron(win, COLOR_PAIR(MD_TUI_COLOR_BORDER));

    /* Draw border */
    box(win, 0, 0);

    /* Draw title */
    if (panel->title[0] != '\0') {
        wattron(win, A_BOLD | COLOR_PAIR(MD_TUI_COLOR_TITLE));
        mvwprintw(win, 0, 2, " %s ", panel->title);
        wattroff(win, A_BOLD | COLOR_PAIR(MD_TUI_COLOR_TITLE));
    }

    /* Highlight if focused */
    if (panel->focused) {
        wattron(win, A_BOLD);
        mvwaddch(win, 0, 1, ACS_RARROW);
        mvwaddch(win, 0, (int)(panel->rect.width - 2), ACS_LARROW);
        wattroff(win, A_BOLD);
    }

    wattroff(win, COLOR_PAIR(MD_TUI_COLOR_BORDER));
}

/**
 * @brief Clear panel content area
 */
MD_UNUSED_FUNCTION static void clear_panel_content(md_tui_panel_t *panel)
{
    WINDOW *win = (WINDOW*)panel->window;
    if (!win) return;

    for (int y = 1; y < panel->rect.height - 1; y++) {
        wmove(win, y, 1);
        for (int x = 1; x < panel->rect.width - 1; x++) {
            waddch(win, ' ');
        }
    }
}

/* ============================================================================
 * Internal Functions - Key Mapping
 * ============================================================================ */

/**
 * @brief Map ncurses key to TUI key
 */
static int map_ncurses_key(int ch)
{
    switch (ch) {
        case KEY_UP:    return MD_KEY_UP;
        case KEY_DOWN:  return MD_KEY_DOWN;
        case KEY_LEFT:  return MD_KEY_LEFT;
        case KEY_RIGHT: return MD_KEY_RIGHT;
        case KEY_HOME:  return MD_KEY_HOME;
        case KEY_END:   return MD_KEY_END;
        case KEY_PPAGE: return MD_KEY_PAGE_UP;
        case KEY_NPAGE: return MD_KEY_PAGE_DOWN;
        case KEY_IC:    return MD_KEY_INSERT;
        case KEY_DC:    return MD_KEY_DELETE;
        case KEY_BTAB:  return MD_KEY_TAB;
        case KEY_BACKSPACE:
        case 127:
        case '\b':      return MD_KEY_BACKSPACE;
        case '\r':
        case '\n':      return MD_KEY_ENTER;
        case 27:        return MD_KEY_ESCAPE;  /* ESC */
        case KEY_F(1):  return MD_KEY_F1;
        case KEY_F(2):  return MD_KEY_F2;
        case KEY_F(3):  return MD_KEY_F3;
        case KEY_F(4):  return MD_KEY_F4;
        case KEY_F(5):  return MD_KEY_F5;
        case KEY_F(6):  return MD_KEY_F6;
        case KEY_F(7):  return MD_KEY_F7;
        case KEY_F(8):  return MD_KEY_F8;
        case KEY_F(9):  return MD_KEY_F9;
        case KEY_F(10): return MD_KEY_F10;
        case KEY_F(11): return MD_KEY_F11;
        case KEY_F(12): return MD_KEY_F12;
        case KEY_RESIZE: return MD_KEY_RESIZE;
        case KEY_MOUSE: return MD_KEY_MOUSE_CLICK;
        default:        return ch;
    }
}

/**
 * @brief Map key to command
 */
static md_tui_command_t map_key_to_command(int key, bool ctrl_held)
{
    /* Function keys */
    if (key >= MD_KEY_F1 && key <= MD_KEY_F12) {
        switch (key) {
            case MD_KEY_F1:  return MD_TUI_CMD_HELP;
            case MD_KEY_F5:  return MD_TUI_CMD_CONTINUE;
            case MD_KEY_F6:  return MD_TUI_CMD_STEP_OVER;
            case MD_KEY_F7:  return MD_TUI_CMD_STEP_INTO;
            case MD_KEY_F8:  return MD_TUI_CMD_STEP_OUT;
            case MD_KEY_F9:  return MD_TUI_CMD_TOGGLE_BREAKPOINT;
            case MD_KEY_F10: return MD_TUI_CMD_STEP_OVER;
            default: return MD_TUI_CMD_NONE;
        }
    }

    /* Control key combinations */
    if (ctrl_held) {
        switch (key) {
            case 'c': return MD_TUI_CMD_CONTINUE;
            case 'n': return MD_TUI_CMD_STEP_OVER;
            case 's': return MD_TUI_CMD_STEP_INTO;
            case 'f': return MD_TUI_CMD_STEP_OUT;
            case 'b': return MD_TUI_CMD_TOGGLE_BREAKPOINT;
            case 'p': return MD_TUI_CMD_GOTO_PC;
            case 'l': return MD_TUI_CMD_REDRAW;
            case 'q': return MD_TUI_CMD_QUIT;
            case 'o': return MD_TUI_CMD_OPEN_FILE;
            case 'r': return MD_TUI_CMD_REFRESH;
            default: return MD_TUI_CMD_NONE;
        }
    }

    /* Regular keys */
    switch (key) {
        case 'h': case MD_KEY_LEFT:  return MD_TUI_CMD_LEFT;
        case 'j': case MD_KEY_DOWN:  return MD_TUI_CMD_DOWN;
        case 'k': case MD_KEY_UP:    return MD_TUI_CMD_UP;
        case 'l': case MD_KEY_RIGHT: return MD_TUI_CMD_RIGHT;

        case 'H': return MD_TUI_CMD_PREV_PANEL;
        case 'L': return MD_TUI_CMD_NEXT_PANEL;
        case 't': return MD_TUI_CMD_NEXT_PANEL;
        case 'T': return MD_TUI_CMD_PREV_PANEL;

        case 'b': return MD_TUI_CMD_TOGGLE_BREAKPOINT;
        case 'B': return MD_TUI_CMD_TOGGLE_BREAKPOINT;
        case 'c': return MD_TUI_CMD_CONTINUE;
        case 'n': return MD_TUI_CMD_STEP_OVER;
        case 's': return MD_TUI_CMD_STEP_INTO;
        case 'f': return MD_TUI_CMD_STEP_OUT;
        case 'r': return MD_TUI_CMD_RESTART;
        case 'q': return MD_TUI_CMD_QUIT;
        case 'Q': return MD_TUI_CMD_FORCE_QUIT;
        case ':': return MD_TUI_CMD_COMMAND_MODE;
        case '?': return MD_TUI_CMD_HELP;
        case 'R': return MD_TUI_CMD_REDRAW;

        case 'p': return MD_TUI_CMD_GOTO_PC;
        case 'P': return MD_TUI_CMD_TOGGLE_ASM;

        case '+': return MD_TUI_CMD_ENABLE_BREAKPOINT;
        case '-': return MD_TUI_CMD_DISABLE_BREAKPOINT;

        case MD_KEY_ESCAPE: return MD_TUI_CMD_NONE;

        default: return MD_TUI_CMD_NONE;
    }
}

/* ============================================================================
 * Configuration Functions
 * ============================================================================ */

MD_API void md_tui_config_init(md_tui_config_t *config)
{
    if (!config) return;

    memset(config, 0, sizeof(md_tui_config_t));

    /* Display defaults */
    config->use_colors = true;
    config->use_utf8 = true;
    config->show_line_numbers = true;
    config->show_breakpoint_marks = true;
    config->show_minimap = false;
    config->tab_width = 4;
    config->source_context_lines = 5;

    /* Behavior defaults */
    config->auto_scroll_output = true;
    config->follow_exec = true;
    config->confirm_quit = true;
    config->refresh_interval_ms = MD_TUI_REFRESH_INTERVAL;
    config->mouse_support = true;

    /* Layout defaults */
    config->source_panel_ratio = 60;
    config->left_panel_ratio = 20;
    config->bottom_panel_ratio = 25;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

MD_API md_tui_t* md_tui_create(struct md_adapter *adapter,
                                const md_tui_config_t *config)
{
    md_tui_t *tui = (md_tui_t*)calloc(1, sizeof(md_tui_t));
    if (!tui) {
        return NULL;
    }

    /* Set configuration */
    if (config) {
        memcpy(&tui->config, config, sizeof(md_tui_config_t));
    } else {
        md_tui_config_init(&tui->config);
    }

    /* Set adapter */
    tui->adapter = adapter;

    /* Initialize state */
    tui->initialized = false;
    tui->running = false;
    tui->needs_redraw = true;
    tui->command_mode = false;

    /* Clear input buffer */
    memset(tui->input_buffer, 0, sizeof(tui->input_buffer));
    tui->input_pos = 0;

    /* Clear message */
    memset(tui->message, 0, sizeof(tui->message));
    tui->message_color = MD_TUI_COLOR_NORMAL;
    tui->message_timeout = 0;

    return tui;
}

MD_API void md_tui_destroy(md_tui_t *tui)
{
    if (!tui) return;

    /* Shutdown ncurses if active */
    if (tui->initialized) {
        md_tui_shutdown(tui);
    }

    /* Free command history */
    for (int i = 0; i < tui->cmd_history_count; i++) {
        free(tui->cmd_history[i]);
    }

    /* Free panels */
    for (int i = 0; i < tui->panel_count; i++) {
        if (tui->panels[i]) {
            if (tui->panels[i]->vtable && tui->panels[i]->vtable->destroy) {
                tui->panels[i]->vtable->destroy(tui->panels[i]);
            }
            free(tui->panels[i]);
        }
    }

    free(tui);
}

MD_API md_error_t md_tui_init(md_tui_t *tui)
{
    if (!tui) return MD_ERROR_NULL_POINTER;
    if (tui->initialized) return MD_ERROR_ALREADY_INITIALIZED;

    /* Check terminal capabilities */
    if (!md_tui_check_terminal()) {
        return MD_ERROR_INVALID_STATE;
    }

    /* Set locale for UTF-8 support */
    setlocale(LC_ALL, "");

    /* Initialize ncurses */
    initscr();

    /* Check screen size */
    /* stdscr is a global in ncurses, for stubs we use defaults */
    tui->screen_width = 80;
    tui->screen_height = 24;
    if (tui->screen_width < MD_TUI_MIN_WIDTH ||
        tui->screen_height < MD_TUI_MIN_HEIGHT) {
        endwin();
        fprintf(stderr, "Terminal too small: %dx%d (min: %dx%d)\n",
                tui->screen_width, tui->screen_height,
                MD_TUI_MIN_WIDTH, MD_TUI_MIN_HEIGHT);
        return MD_ERROR_INVALID_STATE;
    }

    /* Configure ncurses */
    cbreak();               /* Line buffering disabled */
    noecho();               /* Don't echo input */
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);   /* Enable function keys */

    /* Set non-blocking input */
    timeout(GETCH_TIMEOUT_MS);

    /* Initialize colors */
    if (tui->config.use_colors && has_colors()) {
        init_color_pairs();
    }

    /* Enable mouse support */
    if (tui->config.mouse_support) {
        mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
        mouseinterval(50);  /* 50ms click interval */
    }

    /* Initialize panel data */
    memset(s_panel_data, 0, sizeof(s_panel_data));
    s_panel_data_count = 0;

    /* Set cursor invisible */
    curs_set(0);

    tui->initialized = true;
    s_current_tui = tui;

    return MD_SUCCESS;
}

MD_API void md_tui_shutdown(md_tui_t *tui)
{
    if (!tui || !tui->initialized) return;

    /* Hide cursor */
    curs_set(1);

    /* End ncurses mode */
    endwin();

    tui->initialized = false;
    s_current_tui = NULL;
}

/* ============================================================================
 * Main Loop
 * ============================================================================ */

MD_API md_error_t md_tui_run(md_tui_t *tui)
{
    if (!tui) return MD_ERROR_NULL_POINTER;
    if (!tui->initialized) return MD_ERROR_NOT_INITIALIZED;

    tui->running = true;
    tui->needs_redraw = true;

    while (tui->running) {
        /* Process events */
        md_error_t err = md_tui_process_events(tui);
        if (err != MD_SUCCESS && err != MD_ERROR_TIMEOUT) {
            break;
        }

        /* Redraw if needed */
        if (tui->needs_redraw) {
            md_tui_redraw(tui);
            tui->needs_redraw = false;
        }

        /* Update panels */
        update_panels();
        doupdate();
    }

    return MD_SUCCESS;
}

MD_API void md_tui_quit(md_tui_t *tui)
{
    if (tui) {
        tui->running = false;
    }
}

/* ============================================================================
 * Event Processing
 * ============================================================================ */

/**
 * @brief Process a single input key
 */
static void process_input_key(md_tui_t *tui, int key)
{
    /* Check for control key */
    bool ctrl_held = (key & 0x1F) != key && key < 32;

    /* Map to command */
    md_tui_command_t cmd = map_key_to_command(key, ctrl_held);

    /* Handle quit confirmation */
    if (cmd == MD_TUI_CMD_QUIT && tui->config.confirm_quit) {
        md_tui_show_message(tui, MD_TUI_COLOR_WARNING,
                           "Quit? Press 'q' again to confirm, any key to cancel");
        /* Wait for confirmation */
        nodelay(stdscr, FALSE);
        int confirm = getch();
        nodelay(stdscr, TRUE);
        if (confirm != 'q' && confirm != 'Q') {
            md_tui_clear_message(tui);
            return;
        }
    }

    /* Execute command */
    if (cmd != MD_TUI_CMD_NONE) {
        md_tui_execute_command(tui, cmd);
    } else if (tui->focused_panel && tui->focused_panel->vtable &&
               tui->focused_panel->vtable->handle_key) {
        /* Pass to focused panel */
        tui->focused_panel->vtable->handle_key(tui->focused_panel, key);
    }
}

/**
 * @brief Process mouse event
 */
static void process_mouse_event(md_tui_t *tui)
{
    MEVENT mevent;
    if (getmouse(&mevent) != OK) {
        return;
    }

    /* Find panel under mouse */
    md_tui_panel_t *target_panel = NULL;
    for (int i = 0; i < tui->panel_count; i++) {
        md_tui_panel_t *panel = tui->panels[i];
        if (panel->visibility == MD_TUI_VISIBLE &&
            mevent.x >= panel->rect.x &&
            mevent.x < panel->rect.x + panel->rect.width &&
            mevent.y >= panel->rect.y &&
            mevent.y < panel->rect.y + panel->rect.height) {
            target_panel = panel;
            break;
        }
    }

    if (!target_panel) return;

    /* Handle click */
    if (mevent.bstate & BUTTON1_CLICKED) {
        /* Focus panel */
        md_tui_focus_panel(tui, target_panel);

        /* Convert to panel-local coordinates */
        md_tui_mouse_event_t event = {
            .x = mevent.x - target_panel->rect.x - 1,
            .y = mevent.y - target_panel->rect.y - 1,
            .button = 1,
            .ctrl = (mevent.bstate & BUTTON_CTRL) != 0,
            .alt = (mevent.bstate & BUTTON_ALT) != 0,
            .shift = (mevent.bstate & BUTTON_SHIFT) != 0,
            .double_click = false,
        };

        /* Pass to panel */
        if (target_panel->vtable && target_panel->vtable->handle_mouse) {
            target_panel->vtable->handle_mouse(target_panel, &event);
        }
    }
}

/**
 * @brief Process resize event
 */
static void process_resize(md_tui_t *tui)
{
    /* Get new dimensions */
    getmaxyx(stdscr, tui->screen_height, tui->screen_width);

    /* Update layout */
    md_tui_layout_update(tui);

    /* Request full redraw */
    tui->needs_redraw = true;
}

MD_API md_error_t md_tui_process_events(md_tui_t *tui)
{
    if (!tui) return MD_ERROR_NULL_POINTER;

    int ch = getch();

    if (ch == ERR) {
        return MD_ERROR_TIMEOUT;
    }

    /* Map key */
    int key = map_ncurses_key(ch);

    /* Handle special events */
    switch (key) {
        case MD_KEY_RESIZE:
            process_resize(tui);
            break;

        case MD_KEY_MOUSE_CLICK:
            if (tui->config.mouse_support) {
                process_mouse_event(tui);
            }
            break;

        default:
            process_input_key(tui, key);
            break;
    }

    return MD_SUCCESS;
}

/* ============================================================================
 * Command Execution
 * ============================================================================ */

MD_API md_error_t md_tui_execute_command(md_tui_t *tui, md_tui_command_t command)
{
    if (!tui) return MD_ERROR_NULL_POINTER;

    md_error_t err = MD_SUCCESS;

    switch (command) {
        /* Navigation commands */
        case MD_TUI_CMD_UP:
        case MD_TUI_CMD_DOWN:
        case MD_TUI_CMD_LEFT:
        case MD_TUI_CMD_RIGHT:
        case MD_TUI_CMD_PAGE_UP:
        case MD_TUI_CMD_PAGE_DOWN:
        case MD_TUI_CMD_HOME:
        case MD_TUI_CMD_END:
            if (tui->focused_panel && tui->focused_panel->vtable &&
                tui->focused_panel->vtable->handle_key) {
                tui->focused_panel->vtable->handle_key(tui->focused_panel, command);
            }
            break;

        /* Panel management */
        case MD_TUI_CMD_NEXT_PANEL:
            md_tui_focus_next_panel(tui);
            break;
        case MD_TUI_CMD_PREV_PANEL:
            md_tui_focus_prev_panel(tui);
            break;
        case MD_TUI_CMD_FOCUS_SOURCE:
            {
                md_tui_panel_t *panel = md_tui_get_panel(tui, MD_TUI_PANEL_SOURCE);
                if (panel) md_tui_focus_panel(tui, panel);
            }
            break;
        case MD_TUI_CMD_FOCUS_BREAKPOINTS:
            {
                md_tui_panel_t *panel = md_tui_get_panel(tui, MD_TUI_PANEL_BREAKPOINTS);
                if (panel) md_tui_focus_panel(tui, panel);
            }
            break;
        case MD_TUI_CMD_FOCUS_CALLSTACK:
            {
                md_tui_panel_t *panel = md_tui_get_panel(tui, MD_TUI_PANEL_CALLSTACK);
                if (panel) md_tui_focus_panel(tui, panel);
            }
            break;
        case MD_TUI_CMD_FOCUS_VARIABLES:
            {
                md_tui_panel_t *panel = md_tui_get_panel(tui, MD_TUI_PANEL_VARIABLES);
                if (panel) md_tui_focus_panel(tui, panel);
            }
            break;
        case MD_TUI_CMD_FOCUS_OUTPUT:
            {
                md_tui_panel_t *panel = md_tui_get_panel(tui, MD_TUI_PANEL_OUTPUT);
                if (panel) md_tui_focus_panel(tui, panel);
            }
            break;

        /* Debugging control */
        case MD_TUI_CMD_CONTINUE:
            if (tui->adapter) {
                err = md_adapter_continue(tui->adapter);
                if (err == MD_SUCCESS) {
                    md_tui_show_message(tui, MD_TUI_COLOR_SUCCESS, "Running...");
                }
            }
            break;
        case MD_TUI_CMD_STEP_OVER:
            if (tui->adapter) {
                err = md_adapter_step_over(tui->adapter, MD_STEP_LINE);
            }
            break;
        case MD_TUI_CMD_STEP_INTO:
            if (tui->adapter) {
                err = md_adapter_step_into(tui->adapter, MD_STEP_LINE);
            }
            break;
        case MD_TUI_CMD_STEP_OUT:
            if (tui->adapter) {
                err = md_adapter_step_out(tui->adapter);
            }
            break;
        case MD_TUI_CMD_PAUSE:
            if (tui->adapter) {
                err = md_adapter_pause(tui->adapter);
            }
            break;
        case MD_TUI_CMD_RESTART:
            if (tui->adapter) {
                err = md_adapter_restart(tui->adapter);
            }
            break;
        case MD_TUI_CMD_STOP:
            if (tui->adapter) {
                err = md_adapter_disconnect(tui->adapter, true);
                if (err == MD_SUCCESS) {
                    md_tui_show_message(tui, MD_TUI_COLOR_SUCCESS, "Debug session ended");
                }
            }
            break;

        /* Breakpoints */
        case MD_TUI_CMD_TOGGLE_BREAKPOINT:
            /* Toggle breakpoint at current line in source panel */
            if (tui->source_panel && tui->adapter) {
                /* Get current line and file from source panel */
                /* This would be implemented with source panel callbacks */
                md_tui_show_message(tui, MD_TUI_COLOR_NORMAL, "Toggle breakpoint");
            }
            break;

        /* General */
        case MD_TUI_CMD_COMMAND_MODE:
            tui->command_mode = true;
            tui->input_pos = 0;
            tui->input_buffer[0] = '\0';
            curs_set(1);
            break;

        case MD_TUI_CMD_HELP:
            {
                md_tui_panel_t *panel = md_tui_get_panel(tui, MD_TUI_PANEL_HELP);
                if (panel) {
                    md_tui_set_panel_visible(tui, MD_TUI_PANEL_HELP, true);
                    md_tui_focus_panel(tui, panel);
                }
            }
            break;

        case MD_TUI_CMD_QUIT:
        case MD_TUI_CMD_FORCE_QUIT:
            tui->running = false;
            break;

        case MD_TUI_CMD_REDRAW:
            clearok(stdscr, TRUE);
            tui->needs_redraw = true;
            break;

        case MD_TUI_CMD_REFRESH:
            md_tui_update_from_adapter(tui);
            tui->needs_redraw = true;
            break;

        default:
            /* Pass to focused panel */
            if (tui->focused_panel && tui->focused_panel->vtable &&
                tui->focused_panel->vtable->handle_key) {
                tui->focused_panel->vtable->handle_key(tui->focused_panel, command);
            }
            break;
    }

    tui->needs_redraw = true;
    return err;
}

MD_API md_error_t md_tui_execute_command_string(md_tui_t *tui, const char *cmdstr)
{
    if (!tui || !cmdstr) return MD_ERROR_NULL_POINTER;

    /* Look up command by name */
    for (int i = 0; s_command_table[i].name != NULL; i++) {
        if (strcmp(cmdstr, s_command_table[i].name) == 0) {
            return md_tui_execute_command(tui, s_command_table[i].cmd);
        }
    }

    /* Unknown command */
    md_tui_show_message(tui, MD_TUI_COLOR_ERROR, "Unknown command: %s", cmdstr);
    return MD_ERROR_NOT_FOUND;
}

/* ============================================================================
 * Panel Management
 * ============================================================================ */

MD_API md_error_t md_tui_add_panel(md_tui_t *tui, md_tui_panel_t *panel)
{
    if (!tui || !panel) return MD_ERROR_NULL_POINTER;
    if (tui->panel_count >= MD_TUI_MAX_PANELS) return MD_ERROR_BUFFER_OVERFLOW;

    /* Create window */
    WINDOW *win = create_panel_window(panel);
    if (!win) {
        return MD_ERROR_OUT_OF_MEMORY;
    }

    panel->window = win;
    panel->tui = tui;

    /* Create panel */
    PANEL *nc_panel = new_panel(win);
    if (!nc_panel) {
        delwin(win);
        return MD_ERROR_OUT_OF_MEMORY;
    }

    /* Store panel data */
    if (s_panel_data_count < MD_TUI_MAX_PANELS) {
        s_panel_data[s_panel_data_count].win = win;
        s_panel_data[s_panel_data_count].panel = nc_panel;
        s_panel_data[s_panel_data_count].needs_redraw = true;
        panel->id = s_panel_data_count;
        s_panel_data_count++;
    }

    /* Initialize panel via vtable */
    if (panel->vtable && panel->vtable->init) {
        md_error_t err = panel->vtable->init(panel);
        if (err != MD_SUCCESS) {
            del_panel(nc_panel);
            delwin(win);
            return err;
        }
    }

    /* Add to list */
    tui->panels[tui->panel_count++] = panel;

    /* Store special panel references */
    switch (panel->type) {
        case MD_TUI_PANEL_SOURCE:
            tui->source_panel = panel;
            break;
        case MD_TUI_PANEL_STATUS:
            tui->status_panel = panel;
            break;
        case MD_TUI_PANEL_COMMAND:
            tui->command_panel = panel;
            break;
        default:
            break;
    }

    /* Focus first panel */
    if (!tui->focused_panel) {
        md_tui_focus_panel(tui, panel);
    }

    return MD_SUCCESS;
}

MD_API void md_tui_remove_panel(md_tui_t *tui, md_tui_panel_t *panel)
{
    if (!tui || !panel) return;

    /* Find and remove from list */
    for (int i = 0; i < tui->panel_count; i++) {
        if (tui->panels[i] == panel) {
            /* Remove panel data */
            if (panel->id < s_panel_data_count) {
                del_panel(s_panel_data[panel->id].panel);
                delwin(s_panel_data[panel->id].win);
            }

            /* Shift remaining panels */
            for (int j = i; j < tui->panel_count - 1; j++) {
                tui->panels[j] = tui->panels[j + 1];
            }
            tui->panel_count--;

            /* Clear special references */
            if (tui->source_panel == panel) tui->source_panel = NULL;
            if (tui->status_panel == panel) tui->status_panel = NULL;
            if (tui->command_panel == panel) tui->command_panel = NULL;
            if (tui->focused_panel == panel) {
                tui->focused_panel = (tui->panel_count > 0) ?
                                      tui->panels[0] : NULL;
            }
            break;
        }
    }
}

MD_API md_tui_panel_t* md_tui_get_panel(md_tui_t *tui, md_tui_panel_type_t type)
{
    if (!tui) return NULL;

    for (int i = 0; i < tui->panel_count; i++) {
        if (tui->panels[i]->type == type) {
            return tui->panels[i];
        }
    }

    return NULL;
}

MD_API void md_tui_focus_panel(md_tui_t *tui, md_tui_panel_t *panel)
{
    if (!tui || !panel) return;

    /* Unfocus current panel */
    if (tui->focused_panel) {
        tui->focused_panel->focused = false;
        if (tui->focused_panel->vtable && tui->focused_panel->vtable->on_focus) {
            tui->focused_panel->vtable->on_focus(tui->focused_panel, false);
        }
    }

    /* Focus new panel */
    panel->focused = true;
    tui->focused_panel = panel;

    if (panel->vtable && panel->vtable->on_focus) {
        panel->vtable->on_focus(panel, true);
    }

    /* Bring panel to top */
    if (panel->id < s_panel_data_count) {
        top_panel(s_panel_data[panel->id].panel);
    }

    tui->needs_redraw = true;
}

MD_API md_tui_panel_t* md_tui_get_focused_panel(md_tui_t *tui)
{
    return tui ? tui->focused_panel : NULL;
}

MD_API void md_tui_focus_next_panel(md_tui_t *tui)
{
    if (!tui || tui->panel_count == 0) return;

    int idx = 0;
    for (int i = 0; i < tui->panel_count; i++) {
        if (tui->panels[i] == tui->focused_panel) {
            idx = i;
            break;
        }
    }

    /* Find next visible panel */
    for (int i = 1; i < tui->panel_count; i++) {
        int next = (idx + i) % tui->panel_count;
        if (tui->panels[next]->visibility == MD_TUI_VISIBLE) {
            md_tui_focus_panel(tui, tui->panels[next]);
            return;
        }
    }
}

MD_API void md_tui_focus_prev_panel(md_tui_t *tui)
{
    if (!tui || tui->panel_count == 0) return;

    int idx = 0;
    for (int i = 0; i < tui->panel_count; i++) {
        if (tui->panels[i] == tui->focused_panel) {
            idx = i;
            break;
        }
    }

    /* Find previous visible panel */
    for (int i = 1; i < tui->panel_count; i++) {
        int prev = (idx - i + tui->panel_count) % tui->panel_count;
        if (tui->panels[prev]->visibility == MD_TUI_VISIBLE) {
            md_tui_focus_panel(tui, tui->panels[prev]);
            return;
        }
    }
}

MD_API void md_tui_set_panel_visible(md_tui_t *tui,
                                      md_tui_panel_type_t type,
                                      bool visible)
{
    md_tui_panel_t *panel = md_tui_get_panel(tui, type);
    if (!panel) return;

    panel->visibility = visible ? MD_TUI_VISIBLE : MD_TUI_HIDDEN;

    if (panel->id < s_panel_data_count) {
        if (visible) {
            show_panel(s_panel_data[panel->id].panel);
        } else {
            hide_panel(s_panel_data[panel->id].panel);
        }
    }

    md_tui_layout_update(tui);
    tui->needs_redraw = true;
}

/* ============================================================================
 * Layout Management
 * ============================================================================ */

MD_API void md_tui_layout_update(md_tui_t *tui)
{
    if (!tui) return;

    int w = tui->screen_width;
    int h = tui->screen_height;
    int status_h = 1;

    /* Calculate panel dimensions */
    int source_w = w * tui->config.source_panel_ratio / 100;
    int left_w = w * tui->config.left_panel_ratio / 100;
    int bottom_h = h * tui->config.bottom_panel_ratio / 100;
    int main_h = h - bottom_h - status_h - 1;

    /* Layout source panel */
    if (tui->source_panel) {
        md_tui_rect_t rect = {
            .x = left_w + 1,
            .y = 1,
            .width = source_w,
            .height = main_h
        };
        tui->source_panel->rect = rect;
        if (tui->source_panel->vtable && tui->source_panel->vtable->on_resize) {
            tui->source_panel->vtable->on_resize(tui->source_panel, &rect);
        }
    }

    /* Layout status bar */
    if (tui->status_panel) {
        md_tui_rect_t rect = {
            .x = 0,
            .y = h - status_h,
            .width = w,
            .height = status_h
        };
        tui->status_panel->rect = rect;
    }

    /* Resize windows */
    for (int i = 0; i < tui->panel_count; i++) {
        md_tui_panel_t *panel = tui->panels[i];
        if (panel->window && panel->visibility == MD_TUI_VISIBLE) {
            wresize((WINDOW*)panel->window, panel->rect.height, panel->rect.width);
            mvwin((WINDOW*)panel->window, panel->rect.y, panel->rect.x);
        }
    }
}

MD_API void md_tui_set_panel_ratio(md_tui_t *tui,
                                    md_tui_panel_type_t type,
                                    int ratio)
{
    if (!tui) return;

    ratio = MD_MAX(10, MD_MIN(90, ratio));

    switch (type) {
        case MD_TUI_PANEL_SOURCE:
            tui->config.source_panel_ratio = ratio;
            break;
        default:
            break;
    }

    md_tui_layout_update(tui);
}

MD_API md_error_t md_tui_get_panel_rect(md_tui_t *tui,
                                         md_tui_panel_type_t type,
                                         md_tui_rect_t *rect)
{
    if (!tui || !rect) return MD_ERROR_NULL_POINTER;

    md_tui_panel_t *panel = md_tui_get_panel(tui, type);
    if (!panel) return MD_ERROR_NOT_FOUND;

    *rect = panel->rect;
    return MD_SUCCESS;
}

/* ============================================================================
 * Drawing Functions
 * ============================================================================ */

MD_API void md_tui_redraw(md_tui_t *tui)
{
    if (!tui) return;

    /* Clear screen */
    werase(stdscr);

    /* Draw all visible panels */
    for (int i = 0; i < tui->panel_count; i++) {
        md_tui_panel_t *panel = tui->panels[i];
        if (panel->visibility != MD_TUI_VISIBLE) continue;

        /* Draw border and title */
        draw_panel_border(panel);

        /* Render content */
        if (panel->vtable && panel->vtable->render) {
            panel->vtable->render(panel);
        }
    }

    /* Draw status bar */
    md_tui_update_status(tui);

    /* Draw message if present */
    if (tui->message[0] != '\0') {
        attron(COLOR_PAIR(tui->message_color) | A_BOLD);
        mvprintw(tui->screen_height - 1, 0, "%s", tui->message);
        clrtoeol();
        attroff(COLOR_PAIR(tui->message_color) | A_BOLD);
    }

    /* Refresh */
    wnoutrefresh(stdscr);
}

MD_API void md_tui_redraw_panel(md_tui_t *tui, md_tui_panel_t *panel)
{
    if (!tui || !panel || !panel->window) return;

    werase((WINDOW*)panel->window);
    draw_panel_border(panel);

    if (panel->vtable && panel->vtable->render) {
        panel->vtable->render(panel);
    }
}

MD_API void md_tui_update_status(md_tui_t *tui)
{
    if (!tui) return;

    /* Build status string */
    char status[256];
    int pos = 0;

    /* Program name */
    if (tui->adapter) {
        const char *name = md_adapter_get_name(tui->adapter);
        pos += snprintf(status + pos, sizeof(status) - pos, "[%s] ", name);
    }

    /* Execution state */
    if (tui->adapter) {
        md_adapter_state_t state = md_adapter_get_state(tui->adapter);
        const char *state_str = md_adapter_state_string(state);
        pos += snprintf(status + pos, sizeof(status) - pos, "%s", state_str);
    }

    /* File and line */
    if (tui->source_panel) {
        /* Would show current file:line */
    }

    /* Draw status bar */
    attron(COLOR_PAIR(MD_TUI_COLOR_STATUS_BAR) | A_BOLD);
    mvprintw(tui->screen_height - 1, 0, "%-*s", tui->screen_width, status);
    attroff(COLOR_PAIR(MD_TUI_COLOR_STATUS_BAR) | A_BOLD);
}

MD_API void md_tui_show_message(md_tui_t *tui,
                                 md_tui_color_t color,
                                 const char *format, ...)
{
    if (!tui || !format) return;

    va_list args;
    va_start(args, format);
    vsnprintf(tui->message, sizeof(tui->message), format, args);
    va_end(args);

    tui->message_color = color;
    tui->message_timeout = MESSAGE_DURATION_MS;
    tui->needs_redraw = true;
}

MD_API void md_tui_clear_message(md_tui_t *tui)
{
    if (!tui) return;

    tui->message[0] = '\0';
    tui->message_timeout = 0;
    tui->needs_redraw = true;
}

/* ============================================================================
 * Adapter Integration
 * ============================================================================ */

MD_API void md_tui_set_adapter(md_tui_t *tui, struct md_adapter *adapter)
{
    if (tui) {
        tui->adapter = adapter;
    }
}

MD_API struct md_adapter* md_tui_get_adapter(md_tui_t *tui)
{
    return tui ? tui->adapter : NULL;
}

MD_API void md_tui_update_from_adapter(md_tui_t *tui)
{
    if (!tui || !tui->adapter) return;

    /* Update all panels with new state */
    md_adapter_state_t state = md_adapter_get_state(tui->adapter);

    for (int i = 0; i < tui->panel_count; i++) {
        md_tui_panel_t *panel = tui->panels[i];
        if (panel->vtable && panel->vtable->on_state_change) {
            panel->vtable->on_state_change(panel, state);
        }
    }

    tui->needs_redraw = true;
}

/* ============================================================================
 * Input Handling
 * ============================================================================ */

MD_API bool md_tui_handle_event(md_tui_t *tui, const md_tui_event_t *event)
{
    if (!tui || !event) return false;

    switch (event->type) {
        case MD_TUI_EVENT_KEY:
            process_input_key(tui, event->data.key);
            return true;

        case MD_TUI_EVENT_MOUSE:
            if (tui->focused_panel && tui->focused_panel->vtable &&
                tui->focused_panel->vtable->handle_mouse) {
                return tui->focused_panel->vtable->handle_mouse(
                    tui->focused_panel, &event->data.mouse);
            }
            break;

        case MD_TUI_EVENT_RESIZE:
            process_resize(tui);
            return true;

        case MD_TUI_EVENT_COMMAND:
            return md_tui_execute_command(tui, event->data.command) == MD_SUCCESS;

        case MD_TUI_EVENT_STATE_CHANGE:
            md_tui_update_from_adapter(tui);
            return true;

        default:
            break;
    }

    return false;
}

MD_API int md_tui_get_key(void)
{
    int ch = getch();
    return map_ncurses_key(ch);
}

MD_API bool md_tui_is_ctrl_key(int key)
{
    return key < 32 && key > 0;
}

MD_API const char* md_tui_key_name(int key)
{
    if (key >= 32 && key < 127) {
        static char buf[2];
        buf[0] = (char)key;
        buf[1] = '\0';
        return buf;
    }

    switch (key) {
        case MD_KEY_UP:        return "Up";
        case MD_KEY_DOWN:      return "Down";
        case MD_KEY_LEFT:      return "Left";
        case MD_KEY_RIGHT:     return "Right";
        case MD_KEY_HOME:      return "Home";
        case MD_KEY_END:       return "End";
        case MD_KEY_PAGE_UP:   return "PageUp";
        case MD_KEY_PAGE_DOWN: return "PageDown";
        case MD_KEY_INSERT:    return "Insert";
        case MD_KEY_DELETE:    return "Delete";
        case MD_KEY_TAB:       return "Tab";
        case MD_KEY_BACKSPACE: return "Backspace";
        case MD_KEY_ENTER:     return "Enter";
        case MD_KEY_ESCAPE:    return "Escape";
        case MD_KEY_F1:        return "F1";
        case MD_KEY_F2:        return "F2";
        case MD_KEY_F3:        return "F3";
        case MD_KEY_F4:        return "F4";
        case MD_KEY_F5:        return "F5";
        case MD_KEY_F6:        return "F6";
        case MD_KEY_F7:        return "F7";
        case MD_KEY_F8:        return "F8";
        case MD_KEY_F9:        return "F9";
        case MD_KEY_F10:       return "F10";
        case MD_KEY_F11:       return "F11";
        case MD_KEY_F12:       return "F12";
        default:               return "Unknown";
    }
}

/* ============================================================================
 * Command History
 * ============================================================================ */

MD_API void md_tui_add_history(md_tui_t *tui, const char *command)
{
    if (!tui || !command || command[0] == '\0') return;

    /* Don't add duplicates */
    if (tui->cmd_history_count > 0) {
        if (strcmp(tui->cmd_history[tui->cmd_history_count - 1], command) == 0) {
            return;
        }
    }

    /* Shift history if full */
    if (tui->cmd_history_count >= MD_TUI_CMD_HISTORY_MAX) {
        free(tui->cmd_history[0]);
        for (int i = 0; i < tui->cmd_history_count - 1; i++) {
            tui->cmd_history[i] = tui->cmd_history[i + 1];
        }
        tui->cmd_history_count--;
    }

    /* Add new entry */
    tui->cmd_history[tui->cmd_history_count] = md_strdup(command);
    if (tui->cmd_history[tui->cmd_history_count]) {
        tui->cmd_history_count++;
    }
}

MD_API const char* md_tui_get_history(md_tui_t *tui, int index)
{
    if (!tui) return NULL;

    /* Handle negative indexing */
    if (index < 0) {
        index = tui->cmd_history_count + index;
    }

    if (index < 0 || index >= tui->cmd_history_count) {
        return NULL;
    }

    return tui->cmd_history[index];
}

MD_API void md_tui_clear_history(md_tui_t *tui)
{
    if (!tui) return;

    for (int i = 0; i < tui->cmd_history_count; i++) {
        free(tui->cmd_history[i]);
    }
    tui->cmd_history_count = 0;
    tui->cmd_history_index = 0;
}

/* ============================================================================
 * Color Management
 * ============================================================================ */

MD_API void md_tui_init_colors(md_tui_t *tui)
{
    if (!tui) return;
    init_color_pairs();
}

MD_API void md_tui_set_color(md_tui_t *tui, md_tui_color_t color)
{
    if (!tui) return;
    attron(COLOR_PAIR(color));
}

MD_API md_tui_color_t md_tui_get_breakpoint_color(md_tui_t *tui, bool enabled)
{
    MD_UNUSED(tui);
    return enabled ? MD_TUI_COLOR_BREAKPOINT : MD_TUI_COLOR_DISABLED;
}

MD_API md_tui_color_t md_tui_get_thread_color(md_tui_t *tui,
                                               md_thread_state_t state)
{
    MD_UNUSED(tui);
    switch (state) {
        case MD_THREAD_STATE_RUNNING: return MD_TUI_COLOR_SUCCESS;
        case MD_THREAD_STATE_STOPPED: return MD_TUI_COLOR_WARNING;
        case MD_THREAD_STATE_EXITED:  return MD_TUI_COLOR_DISABLED;
        default: return MD_TUI_COLOR_NORMAL;
    }
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

MD_API const char* md_tui_panel_type_name(md_tui_panel_type_t type)
{
    if (type >= 0 && type < (md_tui_panel_type_t)(sizeof(s_panel_type_names) /
                                                    sizeof(s_panel_type_names[0]))) {
        return s_panel_type_names[type];
    }
    return "Unknown";
}

MD_API const char* md_tui_command_name(md_tui_command_t command)
{
    for (int i = 0; s_command_table[i].name != NULL; i++) {
        if (s_command_table[i].cmd == command) {
            return s_command_table[i].name;
        }
    }
    return "unknown";
}

MD_API bool md_tui_check_terminal(void)
{
    /* Check if we have a terminal */
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        return false;
    }

    /* Check for TERM variable */
    const char *term = getenv("TERM");
    if (!term || term[0] == '\0') {
        return false;
    }

    return true;
}

MD_API void md_tui_get_screen_size(int *width, int *height)
{
    if (width) *width = COLS;
    if (height) *height = LINES;
}

MD_API void md_tui_format_breakpoint_marker(int bp_id, bool is_current,
                                             bool enabled, char *buffer, size_t size)
{
    if (!buffer || size == 0) return;

    if (is_current) {
        snprintf(buffer, size, ">");
    } else if (bp_id > 0) {
        snprintf(buffer, size, enabled ? "B" : "b");
    } else {
        snprintf(buffer, size, " ");
    }
}
