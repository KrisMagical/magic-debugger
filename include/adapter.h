/**
 * @file adapter.h
 * @brief Unified Debugger Adapter Interface
 *
 * This file defines the unified interface for debugger adapters,
 * providing a consistent API for different debuggers (LLDB, GDB, Shell).
 *
 * Key Features:
 *   - Breakpoint management (set, remove, enable, disable)
 *   - Execution control (continue, step over, step into, step out)
 *   - Variable inspection and modification
 *   - Source code display with breakpoint markers
 *   - Call stack inspection
 *   - Thread management
 *
 * Phase 4: Adapter Layer - Debugger Integration
 */

#ifndef MAGIC_DEBUGGER_ADAPTER_H
#define MAGIC_DEBUGGER_ADAPTER_H

#include "types.h"
#include "state_types.h"
#include "dap_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

struct md_session;
struct dap_client;
struct md_state_model;
struct md_adapter;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum adapter name length */
#define MD_ADAPTER_NAME_MAX         64

/** Maximum adapter version length */
#define MD_ADAPTER_VERSION_MAX      32

/** Maximum source file line length */
#define MD_ADAPTER_LINE_MAX         4096

/** Maximum number of source lines to display */
#define MD_ADAPTER_SOURCE_CONTEXT   10

/* ============================================================================
 * Adapter Types
 * ============================================================================ */

/**
 * @brief Adapter state
 */
typedef enum md_adapter_state {
    MD_ADAPTER_STATE_NONE       = 0,    /**< Not initialized */
    MD_ADAPTER_STATE_INITIALIZED = 1,   /**< Initialized, ready to launch */
    MD_ADAPTER_STATE_LAUNCHING  = 2,    /**< Launching debuggee */
    MD_ADAPTER_STATE_RUNNING    = 3,    /**< Debuggee running */
    MD_ADAPTER_STATE_STOPPED    = 4,    /**< Debuggee stopped */
    MD_ADAPTER_STATE_TERMINATED = 5,    /**< Debuggee terminated */
    MD_ADAPTER_STATE_ERROR      = 6,    /**< Error state */
} md_adapter_state_t;

/**
 * @brief Step granularity
 */
typedef enum md_step_granularity {
    MD_STEP_LINE        = 0,    /**< Step by source line */
    MD_STEP_STATEMENT   = 1,    /**< Step by statement */
    MD_STEP_INSTRUCTION = 2,    /**< Step by instruction */
} md_step_granularity_t;

/**
 * @brief Breakpoint type for adapter operations
 */
typedef enum md_adapter_bp_type {
    MD_ADAPTER_BP_LINE         = 0,    /**< Line breakpoint */
    MD_ADAPTER_BP_FUNCTION     = 1,    /**< Function breakpoint */
    MD_ADAPTER_BP_CONDITIONAL  = 2,    /**< Conditional breakpoint */
    MD_ADAPTER_BP_WATCHPOINT   = 3,    /**< Watchpoint (data breakpoint) */
    MD_ADAPTER_BP_LOGPOINT     = 4,    /**< Logpoint */
} md_adapter_bp_type_t;

/**
 * @brief Source line information for display
 */
typedef struct md_source_line {
    int line_number;                    /**< Line number (1-based) */
    char content[MD_ADAPTER_LINE_MAX];  /**< Line content */
    bool is_executable;                 /**< Whether this line is executable */
    bool has_breakpoint;                /**< Whether breakpoint exists on this line */
    bool is_current;                    /**< Whether this is current execution line */
    int breakpoint_id;                  /**< Breakpoint ID if has_breakpoint */
    char breakpoint_marker[16];         /**< Breakpoint marker (e.g., ">", "B") */
} md_source_line_t;

/**
 * @brief Source file display context
 */
typedef struct md_source_context {
    char path[MD_MAX_PATH_LEN];         /**< Source file path */
    int current_line;                   /**< Current execution line */
    int start_line;                     /**< First line displayed */
    int end_line;                       /**< Last line displayed */
    int total_lines;                    /**< Total lines in file */
    md_source_line_t *lines;            /**< Array of source lines */
    int line_count;                     /**< Number of lines in array */
} md_source_context_t;

/**
 * @brief Variable evaluation result
 */
typedef struct md_eval_result {
    bool success;                       /**< Whether evaluation succeeded */
    char value[MD_MAX_VARIABLE_STR_LEN]; /**< Variable value */
    char type[256];                     /**< Variable type */
    char error[MD_MAX_ERROR_LEN];       /**< Error message if failed */
    int variables_reference;            /**< Reference for child variables */
    int named_children;                 /**< Number of named children */
    int indexed_children;               /**< Number of indexed children */
} md_eval_result_t;

/* ============================================================================
 * Adapter Configuration
 * ============================================================================ */

/**
 * @brief Adapter configuration
 */
typedef struct md_adapter_config {
    /* Debugger paths */
    char debugger_path[MD_MAX_PATH_LEN];  /**< Path to debugger executable */
    char debugger_args[MD_MAX_PATH_LEN];  /**< Additional debugger arguments */
    
    /* Program configuration */
    char program_path[MD_MAX_PATH_LEN];   /**< Program to debug */
    char program_args[MD_MAX_PATH_LEN];   /**< Program arguments */
    char working_dir[MD_MAX_PATH_LEN];    /**< Working directory */
    char environment[MD_MAX_PATH_LEN];    /**< Environment variables (KEY=VALUE format) */
    
    /* Behavior settings */
    bool stop_on_entry;                    /**< Stop at program entry point */
    bool auto_disassemble;                 /**< Auto-disassemble when no source */
    int source_context_lines;              /**< Lines of context around current line */
    int timeout_ms;                        /**< Default operation timeout */
    int verbosity;                         /**< Verbosity level (0-3) */
} md_adapter_config_t;

/* ============================================================================
 * Adapter Interface (VTable)
 * ============================================================================ */

/**
 * @brief Adapter interface - function pointers for debugger operations
 */
typedef struct md_adapter_interface {
    /* Lifecycle */
    md_error_t (*init)(struct md_adapter *adapter);
    void (*destroy)(struct md_adapter *adapter);
    md_error_t (*launch)(struct md_adapter *adapter);
    md_error_t (*attach)(struct md_adapter *adapter, int pid);
    md_error_t (*disconnect)(struct md_adapter *adapter, bool terminate_debuggee);
    
    /* Breakpoints */
    md_error_t (*set_breakpoint)(struct md_adapter *adapter, const char *path, int line,
                                  const char *condition, int *bp_id);
    md_error_t (*set_function_breakpoint)(struct md_adapter *adapter, const char *function,
                                           int *bp_id);
    md_error_t (*remove_breakpoint)(struct md_adapter *adapter, int bp_id);
    md_error_t (*enable_breakpoint)(struct md_adapter *adapter, int bp_id, bool enable);
    md_error_t (*set_breakpoint_condition)(struct md_adapter *adapter, int bp_id,
                                            const char *condition);
    
    /* Execution Control */
    md_error_t (*continue_exec)(struct md_adapter *adapter);
    md_error_t (*pause)(struct md_adapter *adapter);
    md_error_t (*step_over)(struct md_adapter *adapter, md_step_granularity_t granularity);
    md_error_t (*step_into)(struct md_adapter *adapter, md_step_granularity_t granularity);
    md_error_t (*step_out)(struct md_adapter *adapter);
    md_error_t (*restart)(struct md_adapter *adapter);
    
    /* Variables */
    md_error_t (*evaluate)(struct md_adapter *adapter, const char *expression,
                           int frame_id, md_eval_result_t *result);
    md_error_t (*set_variable)(struct md_adapter *adapter, int var_ref,
                                const char *name, const char *value);
    md_error_t (*get_variables)(struct md_adapter *adapter, int var_ref,
                                 md_variable_t *variables, int max_count, int *actual_count);
    md_error_t (*get_scopes)(struct md_adapter *adapter, int frame_id,
                              md_scope_t *scopes, int max_count, int *actual_count);
    
    /* Stack and Threads */
    md_error_t (*get_threads)(struct md_adapter *adapter, md_thread_t *threads,
                               int max_count, int *actual_count);
    md_error_t (*get_stack_frames)(struct md_adapter *adapter, int thread_id,
                                    md_stack_frame_t *frames, int max_count, int *actual_count);
    md_error_t (*set_active_thread)(struct md_adapter *adapter, int thread_id);
    md_error_t (*set_active_frame)(struct md_adapter *adapter, int frame_id);
    
    /* Source */
    md_error_t (*get_source)(struct md_adapter *adapter, const char *path,
                             int start_line, int end_line, md_source_context_t *ctx);
    md_error_t (*disassemble)(struct md_adapter *adapter, uint64_t address,
                               char *output, size_t output_size);
    
    /* State */
    md_adapter_state_t (*get_state)(struct md_adapter *adapter);
    md_error_t (*get_exception)(struct md_adapter *adapter, md_exception_t *exception);
    md_error_t (*get_debug_state)(struct md_adapter *adapter, md_debug_state_t *state);
    
    /* Process events */
    md_error_t (*process_events)(struct md_adapter *adapter, int timeout_ms);
    
    /* Capabilities */
    bool (*supports_feature)(struct md_adapter *adapter, const char *feature);
    const char* (*get_name)(struct md_adapter *adapter);
    const char* (*get_version)(struct md_adapter *adapter);
} md_adapter_interface_t;

/* ============================================================================
 * Adapter Structure
 * ============================================================================ */

/**
 * @brief Debugger adapter structure
 */
typedef struct md_adapter {
    /* Identity */
    char name[MD_ADAPTER_NAME_MAX];         /**< Adapter name */
    char version[MD_ADAPTER_VERSION_MAX];   /**< Adapter version */
    md_debugger_type_t type;                 /**< Debugger type */
    
    /* Configuration */
    md_adapter_config_t config;              /**< Adapter configuration */
    
    /* State */
    md_adapter_state_t state;                /**< Current adapter state */
    struct md_state_model *state_model;      /**< State model reference */
    
    /* Underlying components */
    struct md_session *session;              /**< Session manager session */
    struct dap_client *dap_client;           /**< DAP client (for DAP-based adapters) */
    
    /* Interface */
    const md_adapter_interface_t *vtable;    /**< Virtual function table */
    
    /* Capabilities */
    dap_capabilities_t capabilities;         /**< Debugger capabilities */
    bool capabilities_loaded;                /**< Whether capabilities are loaded */
    
    /* User data */
    void *user_data;                         /**< User-provided data */
} md_adapter_t;

/* ============================================================================
 * Adapter Registry
 * ============================================================================ */

/**
 * @brief Adapter factory function type
 */
typedef md_adapter_t* (*md_adapter_factory_t)(const md_adapter_config_t *config);

/**
 * @brief Register an adapter type
 * @param type Debugger type
 * @param factory Factory function to create adapter
 * @return Error code
 */
MD_API md_error_t md_adapter_register(md_debugger_type_t type,
                                       md_adapter_factory_t factory);

/**
 * @brief Unregister an adapter type
 * @param type Debugger type
 */
MD_API void md_adapter_unregister(md_debugger_type_t type);

/**
 * @brief Create adapter by type
 * @param type Debugger type
 * @param config Adapter configuration
 * @return Adapter handle, or NULL on error
 */
MD_API md_adapter_t* md_adapter_create(md_debugger_type_t type,
                                        const md_adapter_config_t *config);

/**
 * @brief Create adapter by name
 * @param name Adapter name ("lldb", "gdb", "shell")
 * @param config Adapter configuration
 * @return Adapter handle, or NULL on error
 */
MD_API md_adapter_t* md_adapter_create_by_name(const char *name,
                                                const md_adapter_config_t *config);

/**
 * @brief Destroy an adapter
 * @param adapter Adapter handle
 */
MD_API void md_adapter_destroy(md_adapter_t *adapter);

/* ============================================================================
 * Configuration Helpers
 * ============================================================================ */

/**
 * @brief Initialize adapter configuration with defaults
 * @param config Configuration to initialize
 */
MD_API void md_adapter_config_init(md_adapter_config_t *config);

/**
 * @brief Set program path in configuration
 * @param config Configuration
 * @param path Program path
 * @return Error code
 */
MD_API md_error_t md_adapter_config_set_program(md_adapter_config_t *config,
                                                 const char *path);

/**
 * @brief Set program arguments in configuration
 * @param config Configuration
 * @param args Program arguments (space-separated)
 * @return Error code
 */
MD_API md_error_t md_adapter_config_set_args(md_adapter_config_t *config,
                                              const char *args);

/**
 * @brief Set working directory in configuration
 * @param config Configuration
 * @param dir Working directory
 * @return Error code
 */
MD_API md_error_t md_adapter_config_set_cwd(md_adapter_config_t *config,
                                             const char *dir);

/**
 * @brief Set debugger path in configuration
 * @param config Configuration
 * @param path Debugger executable path
 * @return Error code
 */
MD_API md_error_t md_adapter_config_set_debugger(md_adapter_config_t *config,
                                                  const char *path);

/* ============================================================================
 * Lifecycle Operations
 * ============================================================================ */

/**
 * @brief Initialize adapter (connect to debugger)
 * @param adapter Adapter handle
 * @return Error code
 */
MD_API md_error_t md_adapter_init(md_adapter_t *adapter);

/**
 * @brief Launch the debuggee program
 * @param adapter Adapter handle
 * @return Error code
 */
MD_API md_error_t md_adapter_launch(md_adapter_t *adapter);

/**
 * @brief Attach to a running process
 * @param adapter Adapter handle
 * @param pid Process ID to attach to
 * @return Error code
 */
MD_API md_error_t md_adapter_attach(md_adapter_t *adapter, int pid);

/**
 * @brief Disconnect from debugger
 * @param adapter Adapter handle
 * @param terminate_debuggee Whether to terminate the debuggee
 * @return Error code
 */
MD_API md_error_t md_adapter_disconnect(md_adapter_t *adapter,
                                         bool terminate_debuggee);

/* ============================================================================
 * Breakpoint Operations
 * ============================================================================ */

/**
 * @brief Set a line breakpoint
 * @param adapter Adapter handle
 * @param path Source file path
 * @param line Line number
 * @param condition Optional condition (NULL for unconditional)
 * @param bp_id Pointer to store breakpoint ID
 * @return Error code
 */
MD_API md_error_t md_adapter_set_breakpoint(md_adapter_t *adapter,
                                             const char *path, int line,
                                             const char *condition, int *bp_id);

/**
 * @brief Set a function breakpoint
 * @param adapter Adapter handle
 * @param function Function name
 * @param bp_id Pointer to store breakpoint ID
 * @return Error code
 */
MD_API md_error_t md_adapter_set_function_breakpoint(md_adapter_t *adapter,
                                                      const char *function,
                                                      int *bp_id);

/**
 * @brief Remove a breakpoint
 * @param adapter Adapter handle
 * @param bp_id Breakpoint ID
 * @return Error code
 */
MD_API md_error_t md_adapter_remove_breakpoint(md_adapter_t *adapter, int bp_id);

/**
 * @brief Enable or disable a breakpoint
 * @param adapter Adapter handle
 * @param bp_id Breakpoint ID
 * @param enable Enable state
 * @return Error code
 */
MD_API md_error_t md_adapter_enable_breakpoint(md_adapter_t *adapter,
                                                int bp_id, bool enable);

/**
 * @brief Set breakpoint condition
 * @param adapter Adapter handle
 * @param bp_id Breakpoint ID
 * @param condition Condition expression
 * @return Error code
 */
MD_API md_error_t md_adapter_set_breakpoint_condition(md_adapter_t *adapter,
                                                       int bp_id,
                                                       const char *condition);

/**
 * @brief Set multiple breakpoints in a file
 * @param adapter Adapter handle
 * @param path Source file path
 * @param lines Array of line numbers
 * @param count Number of breakpoints
 * @param bp_ids Array to store breakpoint IDs (can be NULL)
 * @return Error code
 */
MD_API md_error_t md_adapter_set_breakpoints(md_adapter_t *adapter,
                                              const char *path,
                                              const int *lines, int count,
                                              int *bp_ids);

/**
 * @brief Clear all breakpoints
 * @param adapter Adapter handle
 * @return Error code
 */
MD_API md_error_t md_adapter_clear_all_breakpoints(md_adapter_t *adapter);

/* ============================================================================
 * Execution Control
 * ============================================================================ */

/**
 * @brief Continue program execution
 * @param adapter Adapter handle
 * @return Error code
 */
MD_API md_error_t md_adapter_continue(md_adapter_t *adapter);

/**
 * @brief Pause program execution
 * @param adapter Adapter handle
 * @return Error code
 */
MD_API md_error_t md_adapter_pause(md_adapter_t *adapter);

/**
 * @brief Step over (next line)
 * @param adapter Adapter handle
 * @param granularity Step granularity
 * @return Error code
 */
MD_API md_error_t md_adapter_step_over(md_adapter_t *adapter,
                                        md_step_granularity_t granularity);

/**
 * @brief Step into (step into function)
 * @param adapter Adapter handle
 * @param granularity Step granularity
 * @return Error code
 */
MD_API md_error_t md_adapter_step_into(md_adapter_t *adapter,
                                        md_step_granularity_t granularity);

/**
 * @brief Step out (finish current function)
 * @param adapter Adapter handle
 * @return Error code
 */
MD_API md_error_t md_adapter_step_out(md_adapter_t *adapter);

/**
 * @brief Restart the debuggee
 * @param adapter Adapter handle
 * @return Error code
 */
MD_API md_error_t md_adapter_restart(md_adapter_t *adapter);

/**
 * @brief Run to a specific location
 * @param adapter Adapter handle
 * @param path Source file path
 * @param line Line number
 * @return Error code
 */
MD_API md_error_t md_adapter_run_to_location(md_adapter_t *adapter,
                                              const char *path, int line);

/* ============================================================================
 * Variable Operations
 * ============================================================================ */

/**
 * @brief Evaluate an expression
 * @param adapter Adapter handle
 * @param expression Expression to evaluate
 * @param frame_id Frame context (MD_INVALID_ID for global)
 * @param result Pointer to store evaluation result
 * @return Error code
 */
MD_API md_error_t md_adapter_evaluate(md_adapter_t *adapter,
                                       const char *expression,
                                       int frame_id,
                                       md_eval_result_t *result);

/**
 * @brief Set a variable value
 * @param adapter Adapter handle
 * @param var_ref Variable reference
 * @param name Variable name
 * @param value New value (as string)
 * @return Error code
 */
MD_API md_error_t md_adapter_set_variable(md_adapter_t *adapter,
                                           int var_ref,
                                           const char *name,
                                           const char *value);

/**
 * @brief Get variables in a scope
 * @param adapter Adapter handle
 * @param var_ref Variable reference (scope ID)
 * @param variables Array to store variables
 * @param max_count Maximum variables to store
 * @param actual_count Pointer to store actual count
 * @return Error code
 */
MD_API md_error_t md_adapter_get_variables(md_adapter_t *adapter,
                                            int var_ref,
                                            md_variable_t *variables,
                                            int max_count,
                                            int *actual_count);

/**
 * @brief Get scopes for a frame
 * @param adapter Adapter handle
 * @param frame_id Frame ID
 * @param scopes Array to store scopes
 * @param max_count Maximum scopes to store
 * @param actual_count Pointer to store actual count
 * @return Error code
 */
MD_API md_error_t md_adapter_get_scopes(md_adapter_t *adapter,
                                         int frame_id,
                                         md_scope_t *scopes,
                                         int max_count,
                                         int *actual_count);

/**
 * @brief Print variable value (convenience function)
 * @param adapter Adapter handle
 * @param var_name Variable name or expression
 * @param output Buffer for output
 * @param output_size Buffer size
 * @return Error code
 */
MD_API md_error_t md_adapter_print_variable(md_adapter_t *adapter,
                                             const char *var_name,
                                             char *output,
                                             size_t output_size);

/* ============================================================================
 * Stack and Thread Operations
 * ============================================================================ */

/**
 * @brief Get all threads
 * @param adapter Adapter handle
 * @param threads Array to store threads
 * @param max_count Maximum threads to store
 * @param actual_count Pointer to store actual count
 * @return Error code
 */
MD_API md_error_t md_adapter_get_threads(md_adapter_t *adapter,
                                          md_thread_t *threads,
                                          int max_count,
                                          int *actual_count);

/**
 * @brief Get stack frames for a thread
 * @param adapter Adapter handle
 * @param thread_id Thread ID (use current if MD_INVALID_ID)
 * @param frames Array to store frames
 * @param max_count Maximum frames to store
 * @param actual_count Pointer to store actual count
 * @return Error code
 */
MD_API md_error_t md_adapter_get_stack_frames(md_adapter_t *adapter,
                                               int thread_id,
                                               md_stack_frame_t *frames,
                                               int max_count,
                                               int *actual_count);

/**
 * @brief Set active thread
 * @param adapter Adapter handle
 * @param thread_id Thread ID
 * @return Error code
 */
MD_API md_error_t md_adapter_set_active_thread(md_adapter_t *adapter,
                                                int thread_id);

/**
 * @brief Set active stack frame
 * @param adapter Adapter handle
 * @param frame_id Frame ID
 * @return Error code
 */
MD_API md_error_t md_adapter_set_active_frame(md_adapter_t *adapter,
                                               int frame_id);

/**
 * @brief Get current thread ID
 * @param adapter Adapter handle
 * @return Current thread ID, or MD_INVALID_ID
 */
MD_API int md_adapter_get_current_thread_id(md_adapter_t *adapter);

/**
 * @brief Get current frame ID
 * @param adapter Adapter handle
 * @return Current frame ID, or MD_INVALID_ID
 */
MD_API int md_adapter_get_current_frame_id(md_adapter_t *adapter);

/* ============================================================================
 * Source Code Display
 * ============================================================================ */

/**
 * @brief Get source file content
 * @param adapter Adapter handle
 * @param path Source file path
 * @param start_line First line (1-based, 0 for context around current)
 * @param end_line Last line (0 for context around current)
 * @param ctx Pointer to store source context
 * @return Error code
 */
MD_API md_error_t md_adapter_get_source(md_adapter_t *adapter,
                                         const char *path,
                                         int start_line,
                                         int end_line,
                                         md_source_context_t *ctx);

/**
 * @brief Free source context
 * @param ctx Source context to free
 */
MD_API void md_adapter_free_source_context(md_source_context_t *ctx);

/**
 * @brief Disassemble at address
 * @param adapter Adapter handle
 * @param address Memory address
 * @param output Buffer for disassembly output
 * @param output_size Buffer size
 * @return Error code
 */
MD_API md_error_t md_adapter_disassemble(md_adapter_t *adapter,
                                          uint64_t address,
                                          char *output,
                                          size_t output_size);

/**
 * @brief Format source for display
 * @param adapter Adapter handle
 * @param ctx Source context
 * @param output Output buffer
 * @param output_size Buffer size
 * @return Error code
 */
MD_API md_error_t md_adapter_format_source(md_adapter_t *adapter,
                                            const md_source_context_t *ctx,
                                            char *output,
                                            size_t output_size);

/* ============================================================================
 * State Query Operations
 * ============================================================================ */

/**
 * @brief Get adapter state
 * @param adapter Adapter handle
 * @return Adapter state
 */
MD_API md_adapter_state_t md_adapter_get_state(md_adapter_t *adapter);

/**
 * @brief Get adapter state as string
 * @param state Adapter state
 * @return State string
 */
MD_API const char* md_adapter_state_string(md_adapter_state_t state);

/**
 * @brief Get exception information
 * @param adapter Adapter handle
 * @param exception Pointer to store exception
 * @return Error code
 */
MD_API md_error_t md_adapter_get_exception(md_adapter_t *adapter,
                                            md_exception_t *exception);

/**
 * @brief Get complete debug state
 * @param adapter Adapter handle
 * @param state Pointer to store state
 * @return Error code
 */
MD_API md_error_t md_adapter_get_debug_state(md_adapter_t *adapter,
                                              md_debug_state_t *state);

/**
 * @brief Get state model
 * @param adapter Adapter handle
 * @return State model handle
 */
MD_API struct md_state_model* md_adapter_get_state_model(md_adapter_t *adapter);

/* ============================================================================
 * Event Processing
 * ============================================================================ */

/**
 * @brief Process pending events
 * @param adapter Adapter handle
 * @param timeout_ms Timeout in milliseconds
 * @return Error code
 */
MD_API md_error_t md_adapter_process_events(md_adapter_t *adapter,
                                             int timeout_ms);

/**
 * @brief Wait for stop event
 * @param adapter Adapter handle
 * @param timeout_ms Timeout in milliseconds
 * @param reason Pointer to store stop reason (can be NULL)
 * @return Error code
 */
MD_API md_error_t md_adapter_wait_for_stop(md_adapter_t *adapter,
                                            int timeout_ms,
                                            md_stop_reason_t *reason);

/* ============================================================================
 * Capabilities
 * ============================================================================ */

/**
 * @brief Check if adapter supports a feature
 * @param adapter Adapter handle
 * @param feature Feature name
 * @return true if supported
 */
MD_API bool md_adapter_supports_feature(md_adapter_t *adapter,
                                         const char *feature);

/**
 * @brief Get adapter name
 * @param adapter Adapter handle
 * @return Adapter name
 */
MD_API const char* md_adapter_get_name(md_adapter_t *adapter);

/**
 * @brief Get adapter version
 * @param adapter Adapter handle
 * @return Adapter version
 */
MD_API const char* md_adapter_get_version(md_adapter_t *adapter);

/**
 * @brief Get debugger capabilities
 * @param adapter Adapter handle
 * @return Capabilities pointer
 */
MD_API const dap_capabilities_t* md_adapter_get_capabilities(md_adapter_t *adapter);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get adapter state as string
 * @param state Adapter state
 * @return State string
 */
MD_API const char* md_adapter_state_string(md_adapter_state_t state);

/**
 * @brief Get step granularity as string
 * @param granularity Step granularity
 * @return Granularity string
 */
MD_API const char* md_step_granularity_string(md_step_granularity_t granularity);

/**
 * @brief Detect debugger type from path
 * @param path Debugger executable path
 * @return Debugger type
 */
MD_API md_debugger_type_t md_adapter_detect_type(const char *path);

/**
 * @brief Find debugger executable
 * @param type Debugger type
 * @param path Buffer to store path
 * @param path_size Buffer size
 * @return Error code
 */
MD_API md_error_t md_adapter_find_debugger(md_debugger_type_t type,
                                            char *path,
                                            size_t path_size);

#ifdef __cplusplus
}
#endif

#endif /* MAGIC_DEBUGGER_ADAPTER_H */
