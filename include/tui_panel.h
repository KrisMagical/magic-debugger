/**
 * @file tui_panel.h
 * @brief TUI Panel Definitions for Magic Debugger
 *
 * This file defines the panel implementations for the TUI subsystem,
 * including source view, breakpoints, call stack, variables, and more.
 *
 * Key Features:
 *   - Source panel with syntax highlighting and breakpoint markers
 *   - Breakpoint panel with enable/disable toggle
 *   - Call stack panel with frame navigation
 *   - Variable panel with tree expansion
 *   - Output/console panel with logging
 *   - Thread panel for multi-threaded debugging
 *
 * Phase 5: Frontend - TUI Panels
 */

#ifndef MAGIC_DEBUGGER_TUI_PANEL_H
#define MAGIC_DEBUGGER_TUI_PANEL_H

#include "tui.h"
#include "state_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Source Panel
 * ============================================================================ */

/** Maximum source file buffer size */
#define MD_SOURCE_BUFFER_SIZE       (1024 * 1024)

/** Maximum source line length */
#define MD_SOURCE_LINE_MAX          4096

/** Source line display flags */
typedef enum md_source_line_flags {
    MD_SOURCE_LINE_NONE       = 0,
    MD_SOURCE_LINE_BREAKPOINT = 1 << 0,     /**< Has breakpoint */
    MD_SOURCE_LINE_CURRENT    = 1 << 1,     /**< Current execution line */
    MD_SOURCE_LINE_EXECUTABLE = 1 << 2,     /**< Executable code */
    MD_SOURCE_LINE_SELECTED   = 1 << 3,     /**< User selected */
    MD_SOURCE_LINE_BOOKMARK   = 1 << 4,     /**< User bookmark */
} md_source_line_flags_t;

/**
 * @brief Source line data for display
 */
typedef struct md_source_line_info {
    int line_number;                            /**< Line number (1-based) */
    char content[MD_SOURCE_LINE_MAX];           /**< Line content */
    uint32_t flags;                             /**< Display flags */
    int breakpoint_id;                          /**< Breakpoint ID if any */
    bool breakpoint_enabled;                    /**< Breakpoint enabled state */
} md_source_line_info_t;

/**
 * @brief Source panel data
 */
typedef struct md_source_panel_data {
    /* File Info */
    char file_path[MD_MAX_PATH_LEN];            /**< Current file path */
    char **lines;                               /**< Line array */
    int line_count;                             /**< Total lines */
    int total_lines;                            /**< File total lines */

    /* View State */
    int top_line;                               /**< First visible line */
    int cursor_line;                            /**< Cursor position */
    int current_line;                           /**< Current execution line */

    /* Breakpoints */
    int *breakpoint_lines;                      /**< Lines with breakpoints */
    int *breakpoint_ids;                        /**< Breakpoint IDs */
    int breakpoint_count;                       /**< Breakpoint count */

    /* Search */
    char search_pattern[256];                   /**< Search pattern */
    int search_line;                            /**< Current search result line */
    bool search_backward;                       /**< Search direction */

    /* Syntax Highlighting */
    bool syntax_enabled;                        /**< Syntax highlighting on/off */
    char syntax_lang[32];                       /**< Language for highlighting */

    /* Bookmarks */
    int *bookmarks;                             /**< Bookmarked line numbers */
    int bookmark_count;                         /**< Bookmark count */
} md_source_panel_data_t;

/**
 * @brief Create source panel
 * @return Panel handle
 */
MD_API md_tui_panel_t* md_source_panel_create(void);

/**
 * @brief Load source file
 * @param panel Source panel
 * @param path File path
 * @return Error code
 */
MD_API md_error_t md_source_panel_load_file(md_tui_panel_t *panel,
                                             const char *path);

/**
 * @brief Set current execution line
 * @param panel Source panel
 * @param line Line number (0 to clear)
 */
MD_API void md_source_panel_set_current_line(md_tui_panel_t *panel, int line);

/**
 * @brief Scroll to line
 * @param panel Source panel
 * @param line Line number
 * @param center Center line in view
 */
MD_API void md_source_panel_scroll_to_line(md_tui_panel_t *panel,
                                            int line, bool center);

/**
 * @brief Find text in source
 * @param panel Source panel
 * @param pattern Search pattern
 * @param backward Search backward
 * @return Line number found, or -1
 */
MD_API int md_source_panel_find(md_tui_panel_t *panel,
                                 const char *pattern, bool backward);

/**
 * @brief Toggle bookmark at line
 * @param panel Source panel
 * @param line Line number
 */
MD_API void md_source_panel_toggle_bookmark(md_tui_panel_t *panel, int line);

/**
 * @brief Update breakpoints in source view
 * @param panel Source panel
 * @param adapter Debugger adapter
 */
MD_API void md_source_panel_update_breakpoints(md_tui_panel_t *panel,
                                                struct md_adapter *adapter);

/* ============================================================================
 * Breakpoint Panel
 * ============================================================================ */

/** Maximum breakpoints displayable */
#define MD_BREAKPOINTS_MAX         500

/**
 * @brief Breakpoint panel data
 */
typedef struct md_breakpoint_panel_data {
    /* Breakpoint List */
    md_breakpoint_t breakpoints[MD_BREAKPOINTS_MAX];
    int count;

    /* Selection */
    int selected_index;

    /* Filter */
    char filter[256];
    bool show_disabled;
} md_breakpoint_panel_data_t;

/**
 * @brief Create breakpoint panel
 * @return Panel handle
 */
MD_API md_tui_panel_t* md_breakpoint_panel_create(void);

/**
 * @brief Update breakpoint list
 * @param panel Breakpoint panel
 * @param adapter Debugger adapter
 */
MD_API void md_breakpoint_panel_update(md_tui_panel_t *panel,
                                        struct md_adapter *adapter);

/**
 * @brief Get selected breakpoint
 * @param panel Breakpoint panel
 * @return Selected breakpoint, or NULL
 */
MD_API md_breakpoint_t* md_breakpoint_panel_get_selected(md_tui_panel_t *panel);

/**
 * @brief Toggle selected breakpoint
 * @param panel Breakpoint panel
 * @param adapter Debugger adapter
 */
MD_API void md_breakpoint_panel_toggle_selected(md_tui_panel_t *panel,
                                                 struct md_adapter *adapter);

/* ============================================================================
 * Call Stack Panel
 * ============================================================================ */

/** Maximum stack frames displayable */
#define MD_STACK_FRAMES_MAX        100

/**
 * @brief Call stack panel data
 */
typedef struct md_callstack_panel_data {
    /* Stack Frames */
    md_stack_frame_t frames[MD_STACK_FRAMES_MAX];
    int count;

    /* Selection */
    int selected_index;
    int current_frame;          /**< Current execution frame */

    /* Thread info */
    int thread_id;
    char thread_name[64];
} md_callstack_panel_data_t;

/**
 * @brief Create call stack panel
 * @return Panel handle
 */
MD_API md_tui_panel_t* md_callstack_panel_create(void);

/**
 * @brief Update stack frames
 * @param panel Call stack panel
 * @param adapter Debugger adapter
 * @param thread_id Thread ID
 */
MD_API void md_callstack_panel_update(md_tui_panel_t *panel,
                                       struct md_adapter *adapter,
                                       int thread_id);

/**
 * @brief Get selected frame
 * @param panel Call stack panel
 * @return Selected frame, or NULL
 */
MD_API md_stack_frame_t* md_callstack_panel_get_selected(md_tui_panel_t *panel);

/**
 * @brief Select frame
 * @param panel Call stack panel
 * @param adapter Debugger adapter
 * @param frame_index Frame index
 */
MD_API void md_callstack_panel_select_frame(md_tui_panel_t *panel,
                                             struct md_adapter *adapter,
                                             int frame_index);

/* ============================================================================
 * Variable Panel
 * ============================================================================ */

/** Maximum variables displayable */
#define MD_VARIABLES_MAX           500

/** Maximum variable tree depth */
#define MD_VARIABLE_DEPTH_MAX      10

/**
 * @brief Variable display item
 */
typedef struct md_var_display_item {
    md_variable_t var;              /**< Variable data */
    int depth;                      /**< Indentation depth */
    bool expanded;                  /**< Expanded state */
    bool has_children;              /**< Has expandable children */
    bool children_loaded;           /**< Children loaded flag */
} md_var_display_item_t;

/**
 * @brief Variable panel data
 */
typedef struct md_variable_panel_data {
    /* Display Items */
    md_var_display_item_t items[MD_VARIABLES_MAX];
    int count;

    /* Selection */
    int selected_index;

    /* Scope */
    int frame_id;
    char scope_name[32];

    /* Expanded state tracking */
    int expanded_refs[MD_VARIABLES_MAX];
    int expanded_count;

    /* Edit mode */
    bool editing;
    char edit_buffer[256];
    int editing_index;
} md_variable_panel_data_t;

/**
 * @brief Create variable panel
 * @return Panel handle
 */
MD_API md_tui_panel_t* md_variable_panel_create(void);

/**
 * @brief Update variables for frame
 * @param panel Variable panel
 * @param adapter Debugger adapter
 * @param frame_id Frame ID
 */
MD_API void md_variable_panel_update(md_tui_panel_t *panel,
                                      struct md_adapter *adapter,
                                      int frame_id);

/**
 * @brief Expand/collapse variable
 * @param panel Variable panel
 * @param adapter Debugger adapter
 * @param index Item index
 */
MD_API void md_variable_panel_toggle_expand(md_tui_panel_t *panel,
                                             struct md_adapter *adapter,
                                             int index);

/**
 * @brief Set variable value
 * @param panel Variable panel
 * @param adapter Debugger adapter
 * @param index Item index
 * @param value New value string
 * @return Error code
 */
MD_API md_error_t md_variable_panel_set_value(md_tui_panel_t *panel,
                                               struct md_adapter *adapter,
                                               int index,
                                               const char *value);

/* ============================================================================
 * Thread Panel
 * ============================================================================ */

/** Maximum threads displayable */
#define MD_THREADS_MAX             256

/**
 * @brief Thread panel data
 */
typedef struct md_thread_panel_data {
    /* Thread List */
    md_thread_t threads[MD_THREADS_MAX];
    int count;

    /* Selection */
    int selected_index;
    int current_thread;

    /* Filter */
    bool show_stopped_only;
} md_thread_panel_data_t;

/**
 * @brief Create thread panel
 * @return Panel handle
 */
MD_API md_tui_panel_t* md_thread_panel_create(void);

/**
 * @brief Update thread list
 * @param panel Thread panel
 * @param adapter Debugger adapter
 */
MD_API void md_thread_panel_update(md_tui_panel_t *panel,
                                    struct md_adapter *adapter);

/**
 * @brief Select thread
 * @param panel Thread panel
 * @param adapter Debugger adapter
 * @param thread_id Thread ID
 */
MD_API void md_thread_panel_select_thread(md_tui_panel_t *panel,
                                           struct md_adapter *adapter,
                                           int thread_id);

/* ============================================================================
 * Output Panel
 * ============================================================================ */

/** Output buffer size */
#define MD_OUTPUT_BUFFER_SIZE      (64 * 1024)

/** Maximum output lines */
#define MD_OUTPUT_LINES_MAX        10000

/**
 * @brief Output line types
 */
typedef enum md_output_line_type {
    MD_OUTPUT_STDOUT,       /**< Standard output */
    MD_OUTPUT_STDERR,       /**< Standard error */
    MD_OUTPUT_LOG,          /**< Log message */
    MD_OUTPUT_COMMAND,      /**< User command */
    MD_OUTPUT_RESULT,       /**< Command result */
    MD_OUTPUT_ERROR,        /**< Error message */
} md_output_line_type_t;

/**
 * @brief Output line
 */
typedef struct md_output_line {
    char *text;                     /**< Line text */
    md_output_line_type_t type;     /**< Line type */
    uint64_t timestamp;             /**< Timestamp (ms) */
} md_output_line_t;

/**
 * @brief Output panel data
 */
typedef struct md_output_panel_data {
    /* Output Lines */
    md_output_line_t lines[MD_OUTPUT_LINES_MAX];
    int count;
    int start_index;            /**< Circular buffer start */

    /* Scroll State */
    int scroll_offset;
    bool auto_scroll;

    /* Filter */
    char filter[256];
    bool filter_enabled;
} md_output_panel_data_t;

/**
 * @brief Create output panel
 * @return Panel handle
 */
MD_API md_tui_panel_t* md_output_panel_create(void);

/**
 * @brief Add output line
 * @param panel Output panel
 * @param type Line type
 * @param format Printf-style format
 * @param ... Format arguments
 */
MD_API void md_output_panel_add_line(md_tui_panel_t *panel,
                                      md_output_line_type_t type,
                                      const char *format, ...);

/**
 * @brief Clear output
 * @param panel Output panel
 */
MD_API void md_output_panel_clear(md_tui_panel_t *panel);

/**
 * @brief Scroll output
 * @param panel Output panel
 * @param lines Number of lines (negative for up)
 */
MD_API void md_output_panel_scroll(md_tui_panel_t *panel, int lines);

/* ============================================================================
 * Watch Panel
 * ============================================================================ */

/** Maximum watch expressions */
#define MD_WATCH_MAX               50

/**
 * @brief Watch expression item
 */
typedef struct md_watch_item {
    char expression[256];           /**< Expression to watch */
    char value[MD_MAX_VARIABLE_STR_LEN]; /**< Last known value */
    char type[128];                 /**< Value type */
    bool valid;                     /**< Expression is valid */
    bool expanded;                  /**< Expanded state */
    int variables_ref;              /**< Reference for children */
} md_watch_item_t;

/**
 * @brief Watch panel data
 */
typedef struct md_watch_panel_data {
    /* Watch Items */
    md_watch_item_t items[MD_WATCH_MAX];
    int count;

    /* Selection */
    int selected_index;

    /* Edit mode */
    bool editing;
    int editing_index;
    char edit_buffer[256];
} md_watch_panel_data_t;

/**
 * @brief Create watch panel
 * @return Panel handle
 */
MD_API md_tui_panel_t* md_watch_panel_create(void);

/**
 * @brief Add watch expression
 * @param panel Watch panel
 * @param adapter Debugger adapter
 * @param expression Expression to watch
 * @return Error code
 */
MD_API md_error_t md_watch_panel_add(md_tui_panel_t *panel,
                                      struct md_adapter *adapter,
                                      const char *expression);

/**
 * @brief Remove watch expression
 * @param panel Watch panel
 * @param index Item index
 */
MD_API void md_watch_panel_remove(md_tui_panel_t *panel, int index);

/**
 * @brief Update all watch values
 * @param panel Watch panel
 * @param adapter Debugger adapter
 */
MD_API void md_watch_panel_update(md_tui_panel_t *panel,
                                   struct md_adapter *adapter);

/* ============================================================================
 * Disassembly Panel
 * ============================================================================ */

/** Maximum disassembly lines */
#define MD_DISASM_LINES_MAX        200

/**
 * @brief Disassembly line
 */
typedef struct md_disasm_line {
    uint64_t address;               /**< Instruction address */
    char bytes[32];                 /**< Raw bytes (hex) */
    char instruction[128];          /**< Disassembled instruction */
    bool is_pc;                     /**< Is program counter */
} md_disasm_line_t;

/**
 * @brief Disassembly panel data
 */
typedef struct md_disasm_panel_data {
    /* Disassembly Lines */
    md_disasm_line_t lines[MD_DISASM_LINES_MAX];
    int count;

    /* View State */
    uint64_t base_address;
    int scroll_offset;
    int selected_line;

    /* PC position */
    uint64_t pc_address;
    int pc_line;
} md_disasm_panel_data_t;

/**
 * @brief Create disassembly panel
 * @return Panel handle
 */
MD_API md_tui_panel_t* md_disasm_panel_create(void);

/**
 * @brief Disassemble at address
 * @param panel Disassembly panel
 * @param adapter Debugger adapter
 * @param address Starting address
 * @param count Number of instructions
 */
MD_API void md_disasm_panel_disassemble(md_tui_panel_t *panel,
                                         struct md_adapter *adapter,
                                         uint64_t address,
                                         int count);

/**
 * @brief Jump to PC in disassembly
 * @param panel Disassembly panel
 */
MD_API void md_disasm_panel_goto_pc(md_tui_panel_t *panel);

/* ============================================================================
 * Register Panel
 * ============================================================================ */

/** Maximum registers displayable */
#define MD_REGISTERS_MAX           100

/**
 * @brief Register value
 */
typedef struct md_register_value {
    char name[32];                  /**< Register name */
    char value[64];                 /**< Current value */
    char previous[64];              /**< Previous value (for change detection) */
    bool changed;                   /**< Value changed since last stop */
} md_register_value_t;

/**
 * @brief Register panel data
 */
typedef struct md_register_panel_data {
    /* Register Groups */
    char group_name[32];
    md_register_value_t registers[MD_REGISTERS_MAX];
    int count;

    /* Selection */
    int selected_index;

    /* View */
    int scroll_offset;
    bool show_changed_only;
} md_register_panel_data_t;

/**
 * @brief Create register panel
 * @return Panel handle
 */
MD_API md_tui_panel_t* md_register_panel_create(void);

/**
 * @brief Update register values
 * @param panel Register panel
 * @param adapter Debugger adapter
 */
MD_API void md_register_panel_update(md_tui_panel_t *panel,
                                      struct md_adapter *adapter);

/* ============================================================================
 * Status Panel
 * ============================================================================ */

/**
 * @brief Status panel data
 */
typedef struct md_status_panel_data {
    /* Status Items */
    char program_name[256];
    char file_name[256];
    int line_number;
    char function_name[128];

    /* State */
    md_adapter_state_t state;
    char state_detail[256];

    /* Layout info */
    int thread_count;
    int breakpoint_count;
} md_status_panel_data_t;

/**
 * @brief Create status bar panel
 * @return Panel handle
 */
MD_API md_tui_panel_t* md_status_panel_create(void);

/**
 * @brief Update status bar
 * @param panel Status panel
 * @param adapter Debugger adapter
 */
MD_API void md_status_panel_update(md_tui_panel_t *panel,
                                    struct md_adapter *adapter);

/* ============================================================================
 * Help Panel
 * ============================================================================ */

/**
 * @brief Help entry
 */
typedef struct md_help_entry {
    const char *key;                /**< Key binding */
    const char *command;            /**< Command name */
    const char *description;        /**< Description */
} md_help_entry_t;

/**
 * @brief Help panel data
 */
typedef struct md_help_panel_data {
    /* Help entries */
    const md_help_entry_t *entries;
    int count;

    /* Scroll */
    int scroll_offset;
} md_help_panel_data_t;

/**
 * @brief Create help panel
 * @return Panel handle
 */
MD_API md_tui_panel_t* md_help_panel_create(void);

/* ============================================================================
 * Panel Factory
 * ============================================================================ */

/**
 * @brief Create panel by type
 * @param type Panel type
 * @return Panel handle, or NULL
 */
MD_API md_tui_panel_t* md_tui_panel_create_by_type(md_tui_panel_type_t type);

/**
 * @brief Create all standard panels
 * @param tui TUI context
 * @return Error code
 */
MD_API md_error_t md_tui_create_standard_panels(md_tui_t *tui);

#ifdef __cplusplus
}
#endif

#endif /* MAGIC_DEBUGGER_TUI_PANEL_H */
