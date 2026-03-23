/**
 * @file tui.h
 * @brief Terminal User Interface for Magic Debugger
 *
 * This file defines the TUI (Terminal User Interface) subsystem for
 * Magic Debugger, providing an interactive debugging interface using
 * ncurses for terminal rendering.
 *
 * Key Features:
 *   - Multi-panel layout (source, breakpoints, call stack, variables)
 *   - Keyboard-driven navigation and commands
 *   - Real-time state updates
 *   - Syntax highlighting for source code
 *   - Breakpoint markers in source view
 *
 * Phase 5: Frontend - Terminal User Interface
 */

#ifndef MAGIC_DEBUGGER_TUI_H
#define MAGIC_DEBUGGER_TUI_H

#include "types.h"
#include "adapter.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

struct md_adapter;
struct md_tui;
struct md_tui_panel;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of TUI panels */
#define MD_TUI_MAX_PANELS           16

/** Maximum title length for panels */
#define MD_TUI_TITLE_MAX            64

/** Maximum command history */
#define MD_TUI_CMD_HISTORY_MAX      100

/** Maximum input buffer size */
#define MD_TUI_INPUT_MAX            1024

/** Default refresh interval (milliseconds) */
#define MD_TUI_REFRESH_INTERVAL     100

/** Minimum window dimensions */
#define MD_TUI_MIN_WIDTH            80
#define MD_TUI_MIN_HEIGHT           24

/* ============================================================================
 * TUI Color Definitions
 * ============================================================================ */

/**
 * @brief TUI color pairs (foreground, background)
 */
typedef enum md_tui_color {
    MD_TUI_COLOR_DEFAULT          = 0,   /**< Default terminal colors */
    MD_TUI_COLOR_NORMAL           = 1,   /**< Normal text */
    MD_TUI_COLOR_HIGHLIGHT        = 2,   /**< Highlighted text */
    MD_TUI_COLOR_BREAKPOINT       = 3,   /**< Breakpoint line */
    MD_TUI_COLOR_CURRENT_LINE     = 4,   /**< Current execution line */
    MD_TUI_COLOR_STATUS_BAR       = 5,   /**< Status bar */
    MD_TUI_COLOR_ERROR            = 6,   /**< Error messages */
    MD_TUI_COLOR_WARNING          = 7,   /**< Warning messages */
    MD_TUI_COLOR_SUCCESS          = 8,   /**< Success messages */
    MD_TUI_COLOR_KEYWORD          = 9,   /**< Syntax: keywords */
    MD_TUI_COLOR_STRING           = 10,  /**< Syntax: strings */
    MD_TUI_COLOR_COMMENT          = 11,  /**< Syntax: comments */
    MD_TUI_COLOR_NUMBER           = 12,  /**< Syntax: numbers */
    MD_TUI_COLOR_TYPE             = 13,  /**< Syntax: types */
    MD_TUI_COLOR_FUNCTION         = 14,  /**< Syntax: functions */
    MD_TUI_COLOR_VARIABLE         = 15,  /**< Variables in display */
    MD_TUI_COLOR_DISABLED         = 16,  /**< Disabled items */
    MD_TUI_COLOR_SELECTED         = 17,  /**< Selected items */
    MD_TUI_COLOR_BORDER           = 18,  /**< Window borders */
    MD_TUI_COLOR_TITLE            = 19,  /**< Panel titles */
} md_tui_color_t;

/* ============================================================================
 * TUI Panel Types
 * ============================================================================ */

/**
 * @brief Panel types
 */
typedef enum md_tui_panel_type {
    MD_TUI_PANEL_SOURCE       = 0,    /**< Source code panel */
    MD_TUI_PANEL_BREAKPOINTS  = 1,    /**< Breakpoints panel */
    MD_TUI_PANEL_CALLSTACK    = 2,    /**< Call stack panel */
    MD_TUI_PANEL_VARIABLES    = 3,    /**< Variables panel */
    MD_TUI_PANEL_THREADS      = 4,    /**< Threads panel */
    MD_TUI_PANEL_OUTPUT       = 5,    /**< Output/Console panel */
    MD_TUI_PANEL_WATCH        = 6,    /**< Watch expressions panel */
    MD_TUI_PANEL_DISASM       = 7,    /**< Disassembly panel */
    MD_TUI_PANEL_REGISTERS    = 8,    /**< CPU registers panel */
    MD_TUI_PANEL_MEMORY       = 9,    /**< Memory view panel */
    MD_TUI_PANEL_COMMAND      = 10,   /**< Command input panel */
    MD_TUI_PANEL_HELP         = 11,   /**< Help panel */
    MD_TUI_PANEL_STATUS       = 12,   /**< Status bar panel */
} md_tui_panel_type_t;

/**
 * @brief Panel layout position
 */
typedef enum md_tui_layout_pos {
    MD_TUI_LAYOUT_LEFT    = 0,    /**< Left side */
    MD_TUI_LAYOUT_RIGHT   = 1,    /**< Right side */
    MD_TUI_LAYOUT_TOP     = 2,    /**< Top area */
    MD_TUI_LAYOUT_BOTTOM  = 3,    /**< Bottom area */
    MD_TUI_LAYOUT_CENTER  = 4,    /**< Center area (main) */
    MD_TUI_LAYOUT_FLOAT   = 5,    /**< Floating window */
} md_tui_layout_pos_t;

/**
 * @brief Panel visibility state
 */
typedef enum md_tui_visibility {
    MD_TUI_VISIBLE        = 0,    /**< Panel is visible */
    MD_TUI_HIDDEN         = 1,    /**< Panel is hidden */
    MD_TUI_MINIMIZED      = 2,    /**< Panel is minimized */
} md_tui_visibility_t;

/* ============================================================================
 * TUI Key Codes
 * ============================================================================ */

/**
 * @brief Special key codes (beyond standard ASCII)
 */
typedef enum md_tui_key {
    MD_KEY_NONE           = 0,
    MD_KEY_UP             = 0x0100,
    MD_KEY_DOWN           = 0x0101,
    MD_KEY_LEFT           = 0x0102,
    MD_KEY_RIGHT          = 0x0103,
    MD_KEY_HOME           = 0x0104,
    MD_KEY_END            = 0x0105,
    MD_KEY_PAGE_UP        = 0x0106,
    MD_KEY_PAGE_DOWN      = 0x0107,
    MD_KEY_INSERT         = 0x0108,
    MD_KEY_DELETE         = 0x0109,
    MD_KEY_TAB            = 0x010A,
    MD_KEY_BACKSPACE      = 0x010B,
    MD_KEY_ENTER          = 0x010C,
    MD_KEY_ESCAPE         = 0x010D,
    MD_KEY_F1             = 0x0200,
    MD_KEY_F2             = 0x0201,
    MD_KEY_F3             = 0x0202,
    MD_KEY_F4             = 0x0203,
    MD_KEY_F5             = 0x0204,
    MD_KEY_F6             = 0x0205,
    MD_KEY_F7             = 0x0206,
    MD_KEY_F8             = 0x0207,
    MD_KEY_F9             = 0x0208,
    MD_KEY_F10            = 0x0209,
    MD_KEY_F11            = 0x020A,
    MD_KEY_F12            = 0x020B,
    MD_KEY_MOUSE_CLICK    = 0x0300,
    MD_KEY_MOUSE_DBLCLICK = 0x0301,
    MD_KEY_MOUSE_SCROLL   = 0x0302,
    MD_KEY_RESIZE         = 0x0400,
} md_tui_key_t;

/* ============================================================================
 * TUI Commands
 * ============================================================================ */

/**
 * @brief TUI command identifiers
 */
typedef enum md_tui_command {
    /* Navigation */
    MD_TUI_CMD_NONE               = 0,
    MD_TUI_CMD_UP                 = 1,
    MD_TUI_CMD_DOWN               = 2,
    MD_TUI_CMD_LEFT               = 3,
    MD_TUI_CMD_RIGHT              = 4,
    MD_TUI_CMD_PAGE_UP            = 5,
    MD_TUI_CMD_PAGE_DOWN          = 6,
    MD_TUI_CMD_HOME               = 7,
    MD_TUI_CMD_END                = 8,
    MD_TUI_CMD_GOTO_LINE          = 9,

    /* Panel Management */
    MD_TUI_CMD_NEXT_PANEL         = 20,
    MD_TUI_CMD_PREV_PANEL         = 21,
    MD_TUI_CMD_FOCUS_SOURCE       = 22,
    MD_TUI_CMD_FOCUS_BREAKPOINTS  = 23,
    MD_TUI_CMD_FOCUS_CALLSTACK    = 24,
    MD_TUI_CMD_FOCUS_VARIABLES    = 25,
    MD_TUI_CMD_FOCUS_OUTPUT       = 26,
    MD_TUI_CMD_TOGGLE_PANEL       = 27,
    MD_TUI_CMD_ZOOM_PANEL         = 28,

    /* Debugging Control */
    MD_TUI_CMD_CONTINUE           = 40,
    MD_TUI_CMD_STEP_OVER          = 41,
    MD_TUI_CMD_STEP_INTO          = 42,
    MD_TUI_CMD_STEP_OUT           = 43,
    MD_TUI_CMD_PAUSE              = 44,
    MD_TUI_CMD_RESTART            = 45,
    MD_TUI_CMD_STOP               = 46,
    MD_TUI_CMD_RUN_TO_CURSOR      = 47,

    /* Breakpoints */
    MD_TUI_CMD_TOGGLE_BREAKPOINT  = 60,
    MD_TUI_CMD_ADD_BREAKPOINT     = 61,
    MD_TUI_CMD_REMOVE_BREAKPOINT  = 62,
    MD_TUI_CMD_ENABLE_BREAKPOINT  = 63,
    MD_TUI_CMD_DISABLE_BREAKPOINT = 64,
    MD_TUI_CMD_EDIT_BREAKPOINT    = 65,

    /* Variables and Expressions */
    MD_TUI_CMD_EVALUATE           = 80,
    MD_TUI_CMD_WATCH_ADD          = 81,
    MD_TUI_CMD_WATCH_REMOVE       = 82,
    MD_TUI_CMD_SET_VARIABLE       = 83,
    MD_TUI_CMD_PRINT_VARIABLE     = 84,

    /* Stack and Threads */
    MD_TUI_CMD_UP_FRAME           = 100,
    MD_TUI_CMD_DOWN_FRAME         = 101,
    MD_TUI_CMD_SELECT_THREAD      = 102,

    /* Source Navigation */
    MD_TUI_CMD_OPEN_FILE          = 120,
    MD_TUI_CMD_FIND               = 121,
    MD_TUI_CMD_FIND_NEXT          = 122,
    MD_TUI_CMD_FIND_PREV          = 123,
    MD_TUI_CMD_GOTO_PC            = 124,

    /* View Controls */
    MD_TUI_CMD_TOGGLE_ASM         = 140,
    MD_TUI_CMD_TOGGLE_REGISTERS   = 141,
    MD_TUI_CMD_TOGGLE_MEMORY      = 142,
    MD_TUI_CMD_REFRESH            = 143,

    /* General */
    MD_TUI_CMD_COMMAND_MODE       = 160,
    MD_TUI_CMD_HELP               = 161,
    MD_TUI_CMD_QUIT               = 162,
    MD_TUI_CMD_FORCE_QUIT         = 163,
    MD_TUI_CMD_REDRAW             = 164,

    /* Custom command range */
    MD_TUI_CMD_CUSTOM_START       = 1000,
} md_tui_command_t;

/* ============================================================================
 * TUI Event Types
 * ============================================================================ */

/**
 * @brief TUI event types
 */
typedef enum md_tui_event_type {
    MD_TUI_EVENT_KEY          = 1,    /**< Keyboard event */
    MD_TUI_EVENT_MOUSE        = 2,    /**< Mouse event */
    MD_TUI_EVENT_RESIZE       = 3,    /**< Terminal resize */
    MD_TUI_EVENT_COMMAND      = 4,    /**< Command executed */
    MD_TUI_EVENT_STATE_CHANGE = 5,    /**< Debug state changed */
    MD_TUI_EVENT_OUTPUT       = 6,    /**< Debug output received */
    MD_TUI_EVENT_ERROR        = 7,    /**< Error occurred */
    MD_TUI_EVENT_TIMER        = 8,    /**< Timer tick */
    MD_TUI_EVENT_CUSTOM       = 100,  /**< Custom event */
} md_tui_event_type_t;

/**
 * @brief Mouse event data
 */
typedef struct md_tui_mouse_event {
    int x;              /**< X coordinate */
    int y;              /**< Y coordinate */
    int button;         /**< Button number (1=left, 2=middle, 3=right) */
    bool shift;         /**< Shift key held */
    bool ctrl;          /**< Ctrl key held */
    bool alt;           /**< Alt key held */
    bool double_click;  /**< Double click */
} md_tui_mouse_event_t;

/**
 * @brief TUI event structure
 */
typedef struct md_tui_event {
    md_tui_event_type_t type;           /**< Event type */
    union {
        int key;                        /**< Key code for KEY event */
        md_tui_mouse_event_t mouse;     /**< Mouse event data */
        md_tui_command_t command;       /**< Command for COMMAND event */
        struct {
            int width;
            int height;
        } resize;                       /**< Resize dimensions */
        char *output;                   /**< Output text */
        char *error;                    /**< Error message */
        void *custom;                   /**< Custom data */
    } data;
} md_tui_event_t;

/* ============================================================================
 * TUI Panel Definition
 * ============================================================================ */

/**
 * @brief Panel dimensions and position
 */
typedef struct md_tui_rect {
    int x;          /**< X position (column) */
    int y;          /**< Y position (row) */
    int width;      /**< Panel width */
    int height;     /**< Panel height */
} md_tui_rect_t;

/**
 * @brief Panel scroll state
 */
typedef struct md_tui_scroll {
    int offset;         /**< Vertical scroll offset */
    int max_offset;     /**< Maximum scroll offset */
    int cursor_line;    /**< Current cursor line in panel */
} md_tui_scroll_t;

/**
 * @brief Panel interface (vtable)
 */
typedef struct md_tui_panel_interface {
    /* Lifecycle */
    md_error_t (*init)(struct md_tui_panel *panel);
    void (*destroy)(struct md_tui_panel *panel);

    /* Drawing */
    void (*render)(struct md_tui_panel *panel);
    void (*render_line)(struct md_tui_panel *panel, int line);

    /* Input Handling */
    bool (*handle_key)(struct md_tui_panel *panel, int key);
    bool (*handle_mouse)(struct md_tui_panel *panel, const md_tui_mouse_event_t *event);

    /* State Updates */
    void (*on_focus)(struct md_tui_panel *panel, bool focused);
    void (*on_resize)(struct md_tui_panel *panel, const md_tui_rect_t *rect);
    void (*on_state_change)(struct md_tui_panel *panel, md_adapter_state_t state);
    void (*on_breakpoint_change)(struct md_tui_panel *panel);
    void (*on_stack_change)(struct md_tui_panel *panel);
    void (*on_variable_change)(struct md_tui_panel *panel);

    /* Queries */
    const char* (*get_title)(struct md_tui_panel *panel);
    int (*get_line_count)(struct md_tui_panel *panel);
    int (*get_current_line)(struct md_tui_panel *panel);
} md_tui_panel_interface_t;

/**
 * @brief TUI panel structure
 */
typedef struct md_tui_panel {
    /* Identity */
    md_tui_panel_type_t type;               /**< Panel type */
    char title[MD_TUI_TITLE_MAX];           /**< Panel title */
    int id;                                 /**< Unique panel ID */

    /* Layout */
    md_tui_rect_t rect;                     /**< Panel dimensions */
    md_tui_layout_pos_t layout_pos;         /**< Layout position */
    md_tui_visibility_t visibility;         /**< Visibility state */
    bool focused;                           /**< Has keyboard focus */
    bool resizable;                         /**< Can be resized */

    /* Scroll State */
    md_tui_scroll_t scroll;                 /**< Scroll state */

    /* Interface */
    const md_tui_panel_interface_t *vtable; /**< Virtual function table */

    /* Window Handle (ncurses WINDOW*) */
    void *window;                           /**< ncurses window handle */

    /* Parent TUI */
    struct md_tui *tui;                     /**< Parent TUI context */

    /* User Data */
    void *user_data;                        /**< User-provided data */
} md_tui_panel_t;

/* ============================================================================
 * TUI Configuration
 * ============================================================================ */

/**
 * @brief TUI configuration
 */
typedef struct md_tui_config {
    /* Display Settings */
    bool use_colors;                /**< Enable colors */
    bool use_utf8;                  /**< Use UTF-8 characters for borders */
    bool show_line_numbers;         /**< Show line numbers in source */
    bool show_breakpoint_marks;     /**< Show breakpoint markers */
    bool show_minimap;              /**< Show source minimap */
    int tab_width;                  /**< Tab width for source display */
    int source_context_lines;       /**< Context lines around current line */

    /* Behavior Settings */
    bool auto_scroll_output;        /**< Auto-scroll output panel */
    bool follow_exec;               /**< Follow execution in source */
    bool confirm_quit;              /**< Confirm before quitting */
    int refresh_interval_ms;        /**< Refresh interval */
    bool mouse_support;             /**< Enable mouse support */

    /* Layout Settings */
    int source_panel_ratio;         /**< Source panel width ratio (percent) */
    int left_panel_ratio;           /**< Left panels width ratio */
    int bottom_panel_ratio;         /**< Bottom panels height ratio */
} md_tui_config_t;

/* ============================================================================
 * TUI Context
 * ============================================================================ */

/**
 * @brief TUI context structure
 */
typedef struct md_tui {
    /* State */
    bool initialized;               /**< Initialization flag */
    bool running;                   /**< Main loop running flag */
    bool needs_redraw;              /**< Redraw flag */

    /* Configuration */
    md_tui_config_t config;         /**< TUI configuration */

    /* Adapter */
    struct md_adapter *adapter;     /**< Debugger adapter */

    /* Panels */
    md_tui_panel_t *panels[MD_TUI_MAX_PANELS];  /**< Panel array */
    int panel_count;                /**< Number of panels */
    md_tui_panel_t *focused_panel;  /**< Currently focused panel */
    md_tui_panel_t *source_panel;   /**< Source panel reference */
    md_tui_panel_t *status_panel;   /**< Status bar reference */
    md_tui_panel_t *command_panel;  /**< Command input reference */

    /* Input State */
    char input_buffer[MD_TUI_INPUT_MAX];    /**< Input buffer */
    int input_pos;                          /**< Input cursor position */
    bool command_mode;                      /**< Command mode active */

    /* Command History */
    char *cmd_history[MD_TUI_CMD_HISTORY_MAX];  /**< Command history */
    int cmd_history_count;                      /**< History entry count */
    int cmd_history_index;                      /**< Current history index */

    /* Message Display */
    char message[MD_TUI_INPUT_MAX];         /**< Status message */
    md_tui_color_t message_color;           /**< Message color */
    int message_timeout;                    /**< Message timeout (ms) */

    /* Terminal Info */
    int screen_width;               /**< Screen width */
    int screen_height;              /**< Screen height */

    /* User Data */
    void *user_data;                /**< User-provided data */
} md_tui_t;

/* ============================================================================
 * TUI Lifecycle
 * ============================================================================ */

/**
 * @brief Initialize TUI configuration with defaults
 * @param config Configuration to initialize
 */
MD_API void md_tui_config_init(md_tui_config_t *config);

/**
 * @brief Create TUI context
 * @param adapter Debugger adapter (can be NULL)
 * @param config TUI configuration (NULL for defaults)
 * @return TUI context, or NULL on error
 */
MD_API md_tui_t* md_tui_create(struct md_adapter *adapter,
                                const md_tui_config_t *config);

/**
 * @brief Destroy TUI context
 * @param tui TUI context
 */
MD_API void md_tui_destroy(md_tui_t *tui);

/**
 * @brief Initialize TUI (enter ncurses mode)
 * @param tui TUI context
 * @return Error code
 */
MD_API md_error_t md_tui_init(md_tui_t *tui);

/**
 * @brief Shutdown TUI (exit ncurses mode)
 * @param tui TUI context
 */
MD_API void md_tui_shutdown(md_tui_t *tui);

/**
 * @brief Run TUI main loop
 * @param tui TUI context
 * @return Error code (returns when TUI exits)
 */
MD_API md_error_t md_tui_run(md_tui_t *tui);

/**
 * @brief Request TUI to exit
 * @param tui TUI context
 */
MD_API void md_tui_quit(md_tui_t *tui);

/* ============================================================================
 * Adapter Integration
 * ============================================================================ */

/**
 * @brief Set debugger adapter
 * @param tui TUI context
 * @param adapter Debugger adapter
 */
MD_API void md_tui_set_adapter(md_tui_t *tui, struct md_adapter *adapter);

/**
 * @brief Get debugger adapter
 * @param tui TUI context
 * @return Debugger adapter
 */
MD_API struct md_adapter* md_tui_get_adapter(md_tui_t *tui);

/**
 * @brief Update TUI from adapter state
 * @param tui TUI context
 */
MD_API void md_tui_update_from_adapter(md_tui_t *tui);

/* ============================================================================
 * Panel Management
 * ============================================================================ */

/**
 * @brief Register a panel
 * @param tui TUI context
 * @param panel Panel to register
 * @return Error code
 */
MD_API md_error_t md_tui_add_panel(md_tui_t *tui, md_tui_panel_t *panel);

/**
 * @brief Remove a panel
 * @param tui TUI context
 * @param panel Panel to remove
 */
MD_API void md_tui_remove_panel(md_tui_t *tui, md_tui_panel_t *panel);

/**
 * @brief Get panel by type
 * @param tui TUI context
 * @param type Panel type
 * @return Panel, or NULL if not found
 */
MD_API md_tui_panel_t* md_tui_get_panel(md_tui_t *tui, md_tui_panel_type_t type);

/**
 * @brief Focus a panel
 * @param tui TUI context
 * @param panel Panel to focus
 */
MD_API void md_tui_focus_panel(md_tui_t *tui, md_tui_panel_t *panel);

/**
 * @brief Get focused panel
 * @param tui TUI context
 * @return Focused panel
 */
MD_API md_tui_panel_t* md_tui_get_focused_panel(md_tui_t *tui);

/**
 * @brief Focus next panel
 * @param tui TUI context
 */
MD_API void md_tui_focus_next_panel(md_tui_t *tui);

/**
 * @brief Focus previous panel
 * @param tui TUI context
 */
MD_API void md_tui_focus_prev_panel(md_tui_t *tui);

/**
 * @brief Show/hide panel
 * @param tui TUI context
 * @param type Panel type
 * @param visible Visibility state
 */
MD_API void md_tui_set_panel_visible(md_tui_t *tui,
                                      md_tui_panel_type_t type,
                                      bool visible);

/* ============================================================================
 * Layout Management
 * ============================================================================ */

/**
 * @brief Recalculate layout
 * @param tui TUI context
 */
MD_API void md_tui_layout_update(md_tui_t *tui);

/**
 * @brief Set panel ratio
 * @param tui TUI context
 * @param type Panel type
 * @param ratio Size ratio (percent)
 */
MD_API void md_tui_set_panel_ratio(md_tui_t *tui,
                                    md_tui_panel_type_t type,
                                    int ratio);

/**
 * @brief Get panel rectangle
 * @param tui TUI context
 * @param type Panel type
 * @param rect Pointer to store rectangle
 * @return Error code
 */
MD_API md_error_t md_tui_get_panel_rect(md_tui_t *tui,
                                         md_tui_panel_type_t type,
                                         md_tui_rect_t *rect);

/* ============================================================================
 * Drawing Functions
 * ============================================================================ */

/**
 * @brief Request full redraw
 * @param tui TUI context
 */
MD_API void md_tui_redraw(md_tui_t *tui);

/**
 * @brief Redraw a specific panel
 * @param tui TUI context
 * @param panel Panel to redraw
 */
MD_API void md_tui_redraw_panel(md_tui_t *tui, md_tui_panel_t *panel);

/**
 * @brief Update status bar
 * @param tui TUI context
 */
MD_API void md_tui_update_status(md_tui_t *tui);

/**
 * @brief Show a message
 * @param tui TUI context
 * @param color Message color
 * @param format Printf-style format string
 * @param ... Format arguments
 */
MD_API void md_tui_show_message(md_tui_t *tui,
                                 md_tui_color_t color,
                                 const char *format, ...);

/**
 * @brief Clear message
 * @param tui TUI context
 */
MD_API void md_tui_clear_message(md_tui_t *tui);

/* ============================================================================
 * Input Handling
 * ============================================================================ */

/**
 * @brief Process input event
 * @param tui TUI context
 * @param event Event to process
 * @return true if event was handled
 */
MD_API bool md_tui_handle_event(md_tui_t *tui, const md_tui_event_t *event);

/**
 * @brief Get key code from ncurses
 * @return Key code
 */
MD_API int md_tui_get_key(void);

/**
 * @brief Check if key is a control character
 * @param key Key code
 * @return true if control character
 */
MD_API bool md_tui_is_ctrl_key(int key);

/**
 * @brief Get key name string
 * @param key Key code
 * @return Key name string
 */
MD_API const char* md_tui_key_name(int key);

/**
 * @brief Execute a TUI command
 * @param tui TUI context
 * @param command Command to execute
 * @return Error code
 */
MD_API md_error_t md_tui_execute_command(md_tui_t *tui, md_tui_command_t command);

/**
 * @brief Execute a command string
 * @param tui TUI context
 * @param cmdstr Command string
 * @return Error code
 */
MD_API md_error_t md_tui_execute_command_string(md_tui_t *tui, const char *cmdstr);

/* ============================================================================
 * Command History
 * ============================================================================ */

/**
 * @brief Add command to history
 * @param tui TUI context
 * @param command Command string
 */
MD_API void md_tui_add_history(md_tui_t *tui, const char *command);

/**
 * @brief Get history entry
 * @param tui TUI context
 * @param index History index (negative for relative to current)
 * @return Command string, or NULL
 */
MD_API const char* md_tui_get_history(md_tui_t *tui, int index);

/**
 * @brief Clear command history
 * @param tui TUI context
 */
MD_API void md_tui_clear_history(md_tui_t *tui);

/* ============================================================================
 * Color Management
 * ============================================================================ */

/**
 * @brief Initialize colors
 * @param tui TUI context
 */
MD_API void md_tui_init_colors(md_tui_t *tui);

/**
 * @brief Set color attribute
 * @param tui TUI context
 * @param color Color pair
 */
MD_API void md_tui_set_color(md_tui_t *tui, md_tui_color_t color);

/**
 * @brief Get color for breakpoint state
 * @param tui TUI context
 * @param enabled Breakpoint enabled state
 * @return Color pair
 */
MD_API md_tui_color_t md_tui_get_breakpoint_color(md_tui_t *tui, bool enabled);

/**
 * @brief Get color for thread state
 * @param tui TUI context
 * @param state Thread state
 * @return Color pair
 */
MD_API md_tui_color_t md_tui_get_thread_color(md_tui_t *tui,
                                               md_thread_state_t state);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get panel type name
 * @param type Panel type
 * @return Type name string
 */
MD_API const char* md_tui_panel_type_name(md_tui_panel_type_t type);

/**
 * @brief Get command name
 * @param command Command
 * @return Command name string
 */
MD_API const char* md_tui_command_name(md_tui_command_t command);

/**
 * @brief Check terminal capabilities
 * @param tui TUI context
 * @return true if terminal supports required features
 */
MD_API bool md_tui_check_terminal(void);

/**
 * @brief Get current screen dimensions
 * @param width Pointer to store width
 * @param height Pointer to store height
 */
MD_API void md_tui_get_screen_size(int *width, int *height);

/**
 * @brief Format breakpoint marker
 * @param bp_id Breakpoint ID
 * @param is_current Is current line
 * @param enabled Is breakpoint enabled
 * @param buffer Output buffer
 * @param size Buffer size
 */
MD_API void md_tui_format_breakpoint_marker(int bp_id, bool is_current,
                                             bool enabled, char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* MAGIC_DEBUGGER_TUI_H */
