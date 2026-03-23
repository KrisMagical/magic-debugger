/**
 * @file state_types.h
 * @brief Unified Debug State Type Definitions for Magic Debugger
 * 
 * This file contains unified type definitions that abstract away differences
 * between different debuggers (GDB, LLDB, etc.) and provide a consistent
 * representation of debug state.
 * 
 * Phase 3: State Model - Unified Debug State Representation
 */

#ifndef MAGIC_DEBUGGER_STATE_TYPES_H
#define MAGIC_DEBUGGER_STATE_TYPES_H

#include "types.h"
#include "dap_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of breakpoints per session */
#define MD_MAX_BREAKPOINTS          1024

/** Maximum number of stack frames */
#define MD_MAX_STACK_FRAMES         256

/** Maximum number of threads */
#define MD_MAX_THREADS              128

/** Maximum number of scopes per frame */
#define MD_MAX_SCOPES               16

/** Maximum number of variables per scope */
#define MD_MAX_VARIABLES            1024

/** Maximum variable name/value length */
#define MD_MAX_VARIABLE_STR_LEN     4096

/** Maximum number of modules */
#define MD_MAX_MODULES              256

/** Invalid ID constant */
#define MD_INVALID_ID               (-1)

/* ============================================================================
 * Execution State
 * ============================================================================ */

/**
 * @brief Program execution state
 * 
 * This represents the high-level state of the debuggee program,
 * abstracting away debugger-specific states.
 */
typedef enum md_exec_state {
    MD_EXEC_STATE_NONE       = 0,    /**< No program loaded */
    MD_EXEC_STATE_LAUNCHING  = 1,    /**< Program is being launched */
    MD_EXEC_STATE_RUNNING    = 2,    /**< Program is running */
    MD_EXEC_STATE_STOPPED    = 3,    /**< Program is stopped (at breakpoint, etc.) */
    MD_EXEC_STATE_STEPPING   = 4,    /**< Program is stepping */
    MD_EXEC_STATE_EXITING    = 5,    /**< Program is exiting */
    MD_EXEC_STATE_TERMINATED = 6,    /**< Program has terminated */
    MD_EXEC_STATE_CRASHED    = 7,    /**< Program crashed */
} md_exec_state_t;

/**
 * @brief Get string representation of execution state
 */
MD_API const char* md_exec_state_string(md_exec_state_t state);

/**
 * @brief Check if program is in a "stopped" state (can inspect)
 */
MD_API bool md_exec_state_is_stopped(md_exec_state_t state);

/**
 * @brief Check if program is in a "running" state
 */
MD_API bool md_exec_state_is_running(md_exec_state_t state);

/**
 * @brief Check if program has terminated
 */
MD_API bool md_exec_state_is_terminated(md_exec_state_t state);

/* ============================================================================
 * Stop Reason
 * ============================================================================ */

/**
 * @brief Unified stop reason
 * 
 * This represents why the program stopped execution,
 * mapping DAP stop reasons to unified representation.
 */
typedef enum md_stop_reason {
    MD_STOP_REASON_UNKNOWN            = 0,   /**< Unknown reason */
    MD_STOP_REASON_STEP               = 1,   /**< Step completed */
    MD_STOP_REASON_BREAKPOINT         = 2,   /**< Breakpoint hit */
    MD_STOP_REASON_EXCEPTION          = 3,   /**< Exception thrown */
    MD_STOP_REASON_PAUSE              = 4,   /**< User paused */
    MD_STOP_REASON_ENTRY              = 5,   /**< Entry point reached */
    MD_STOP_REASON_GOTO               = 6,   /**< Goto target reached */
    MD_STOP_REASON_FUNCTION_BREAKPOINT = 7,  /**< Function breakpoint hit */
    MD_STOP_REASON_DATA_BREAKPOINT    = 8,   /**< Data breakpoint hit */
    MD_STOP_REASON_INSTRUCTION_BREAKPOINT = 9, /**< Instruction breakpoint hit */
    MD_STOP_REASON_SIGNAL             = 10,  /**< Signal received */
    MD_STOP_REASON_WATCHPOINT         = 11,  /**< Watchpoint hit */
} md_stop_reason_t;

/**
 * @brief Get string representation of stop reason
 */
MD_API const char* md_stop_reason_string(md_stop_reason_t reason);

/**
 * @brief Convert DAP stop reason to unified stop reason
 */
MD_API md_stop_reason_t md_stop_reason_from_dap(dap_stop_reason_t dap_reason);

/**
 * @brief Parse stop reason from string
 */
MD_API md_stop_reason_t md_stop_reason_from_string(const char *str);

/* ============================================================================
 * Breakpoint State
 * ============================================================================ */

/**
 * @brief Breakpoint state
 */
typedef enum md_bp_state {
    MD_BP_STATE_INVALID      = 0,    /**< Invalid/uninitialized breakpoint */
    MD_BP_STATE_PENDING      = 1,    /**< Breakpoint pending verification */
    MD_BP_STATE_VERIFIED     = 2,    /**< Breakpoint verified */
    MD_BP_STATE_FAILED       = 3,    /**< Breakpoint verification failed */
    MD_BP_STATE_REMOVED      = 4,    /**< Breakpoint removed */
    MD_BP_STATE_DISABLED     = 5,    /**< Breakpoint disabled */
} md_bp_state_t;

/**
 * @brief Get string representation of breakpoint state
 */
MD_API const char* md_bp_state_string(md_bp_state_t state);

/**
 * @brief Breakpoint type
 */
typedef enum md_bp_type {
    MD_BP_TYPE_LINE          = 0,    /**< Line breakpoint */
    MD_BP_TYPE_FUNCTION      = 1,    /**< Function breakpoint */
    MD_BP_TYPE_CONDITIONAL   = 2,    /**< Conditional breakpoint */
    MD_BP_TYPE_DATA          = 3,    /**< Data/watch breakpoint */
    MD_BP_TYPE_INSTRUCTION   = 4,    /**< Instruction breakpoint */
    MD_BP_TYPE_LOGPOINT      = 5,    /**< Logpoint */
} md_bp_type_t;

/**
 * @brief Get string representation of breakpoint type
 */
MD_API const char* md_bp_type_string(md_bp_type_t type);

/**
 * @brief Unified breakpoint structure
 */
typedef struct md_breakpoint {
    /* Identity */
    int id;                         /**< Internal breakpoint ID */
    int dap_id;                     /**< DAP breakpoint ID (from adapter) */
    
    /* Type and State */
    md_bp_type_t type;              /**< Breakpoint type */
    md_bp_state_t state;            /**< Current state */
    
    /* Location */
    char path[MD_MAX_PATH_LEN];     /**< Source file path */
    int line;                       /**< Line number */
    int column;                     /**< Column number (0 if not set) */
    char function_name[256];        /**< Function name (for function breakpoints) */
    
    /* Conditions */
    char condition[MD_MAX_VARIABLE_STR_LEN];     /**< Condition expression */
    char hit_condition[256];        /**< Hit condition */
    char log_message[MD_MAX_VARIABLE_STR_LEN];   /**< Log message (for logpoints) */
    int hit_count;                  /**< Current hit count */
    int ignore_count;               /**< Ignore count */
    
    /* Status */
    bool enabled;                   /**< Whether breakpoint is enabled */
    bool verified;                  /**< Whether breakpoint is verified */
    char message[MD_MAX_ERROR_LEN]; /**< Verification message */
    
    /* Adapter-specific data */
    void *adapter_data;             /**< Debugger adapter private data */
} md_breakpoint_t;

/* ============================================================================
 * Thread State
 * ============================================================================ */

/**
 * @brief Thread state
 */
typedef enum md_thread_state {
    MD_THREAD_STATE_UNKNOWN  = 0,    /**< Unknown state */
    MD_THREAD_STATE_STOPPED  = 1,    /**< Thread is stopped */
    MD_THREAD_STATE_RUNNING  = 2,    /**< Thread is running */
    MD_THREAD_STATE_STEPPING = 3,    /**< Thread is stepping */
    MD_THREAD_STATE_EXITED   = 4,    /**< Thread has exited */
} md_thread_state_t;

/**
 * @brief Get string representation of thread state
 */
MD_API const char* md_thread_state_string(md_thread_state_t state);

/**
 * @brief Unified thread structure
 */
typedef struct md_thread {
    int id;                         /**< Thread ID */
    char name[256];                 /**< Thread name */
    md_thread_state_t state;        /**< Thread state */
    int current_frame_id;           /**< Current stack frame ID */
    bool is_stopped;                /**< Whether thread is stopped */
    bool is_main;                   /**< Whether this is the main thread */
} md_thread_t;

/* ============================================================================
 * Stack Frame
 * ============================================================================ */

/**
 * @brief Unified stack frame structure
 */
typedef struct md_stack_frame {
    /* Identity */
    int id;                         /**< Frame ID (DAP frame ID) */
    
    /* Location */
    char name[256];                 /**< Function name */
    char module[256];               /**< Module/shared library name */
    char source_path[MD_MAX_PATH_LEN]; /**< Source file path */
    int line;                       /**< Line number */
    int column;                     /**< Column number */
    int end_line;                   /**< End line (0 if not set) */
    int end_column;                 /**< End column (0 if not set) */
    
    /* Address information */
    uint64_t instruction_pointer;   /**< Instruction pointer address */
    uint64_t return_address;        /**< Return address */
    
    /* Presentation */
    char presentation_hint[32];     /**< "normal", "label", "subtle" */
    bool is_user_code;              /**< Whether this is user code */
    
    /* Thread association */
    int thread_id;                  /**< Thread this frame belongs to */
} md_stack_frame_t;

/* ============================================================================
 * Variable
 * ============================================================================ */

/**
 * @brief Variable scope type
 */
typedef enum md_scope_type {
    MD_SCOPE_TYPE_LOCALS    = 0,    /**< Local variables */
    MD_SCOPE_TYPE_ARGUMENTS = 1,    /**< Function arguments */
    MD_SCOPE_TYPE_GLOBALS   = 2,    /**< Global variables */
    MD_SCOPE_TYPE_REGISTERS = 3,    /**< CPU registers */
    MD_SCOPE_TYPE_CLOSURE   = 4,    /**< Closure variables */
    MD_SCOPE_TYPE_RETURN    = 5,    /**< Return value */
} md_scope_type_t;

/**
 * @brief Get string representation of scope type
 */
MD_API const char* md_scope_type_string(md_scope_type_t type);

/**
 * @brief Unified scope structure
 */
typedef struct md_scope {
    int id;                         /**< Scope ID (DAP variables reference) */
    md_scope_type_t type;           /**< Scope type */
    char name[64];                  /**< Scope name */
    int frame_id;                   /**< Frame this scope belongs to */
    int named_variables;            /**< Number of named variables */
    int indexed_variables;          /**< Number of indexed variables */
    bool expensive;                 /**< Whether fetching is expensive */
} md_scope_t;

/**
 * @brief Variable presentation hint
 */
typedef enum md_var_kind {
    MD_VAR_KIND_NORMAL      = 0,    /**< Normal variable */
    MD_VAR_KIND_POINTER     = 1,    /**< Pointer */
    MD_VAR_KIND_ARRAY       = 2,    /**< Array */
    MD_VAR_KIND_OBJECT      = 3,    /**< Object/struct */
    MD_VAR_KIND_COLLECTION  = 4,    /**< Collection (list, map, etc.) */
    MD_VAR_KIND_STRING      = 5,    /**< String */
    MD_VAR_KIND_NULL        = 6,    /**< Null/nil value */
    MD_VAR_KIND_FUNCTION    = 7,    /**< Function/closure */
    MD_VAR_KIND_VIRTUAL     = 8,    /**< Virtual/synthetic variable */
} md_var_kind_t;

/**
 * @brief Get string representation of variable kind
 */
MD_API const char* md_var_kind_string(md_var_kind_t kind);

/**
 * @brief Unified variable structure
 */
typedef struct md_variable {
    /* Identity */
    int id;                         /**< Variable ID (for child references) */
    
    /* Basic info */
    char name[256];                 /**< Variable name */
    char value[MD_MAX_VARIABLE_STR_LEN]; /**< Variable value string */
    char type[256];                 /**< Variable type name */
    
    /* Hierarchy */
    int variables_reference;        /**< Reference to child variables (0 if none) */
    int named_children;             /**< Number of named children */
    int indexed_children;           /**< Number of indexed children */
    int parent_id;                  /**< Parent variable ID (0 if none) */
    
    /* Evaluation */
    char evaluate_name[MD_MAX_VARIABLE_STR_LEN]; /**< Expression to evaluate this variable */
    
    /* Presentation */
    md_var_kind_t kind;             /**< Variable kind */
    char presentation_hint[32];     /**< Presentation hint */
    uint64_t memory_reference;      /**< Memory address (if applicable) */
    
    /* Scope */
    int scope_id;                   /**< Scope this variable belongs to */
} md_variable_t;

/* ============================================================================
 * Module
 * ============================================================================ */

/**
 * @brief Module state
 */
typedef enum md_module_state {
    MD_MODULE_STATE_UNKNOWN     = 0, /**< Unknown state */
    MD_MODULE_STATE_LOADING     = 1, /**< Module is loading */
    MD_MODULE_STATE_LOADED      = 2, /**< Module is loaded */
    MD_MODULE_STATE_FAILED      = 3, /**< Module failed to load */
    MD_MODULE_STATE_UNLOADED    = 4, /**< Module is unloaded */
} md_module_state_t;

/**
 * @brief Get string representation of module state
 */
MD_API const char* md_module_state_string(md_module_state_t state);

/**
 * @brief Unified module structure
 */
typedef struct md_module {
    int id;                         /**< Module ID */
    char name[256];                 /**< Module name */
    char path[MD_MAX_PATH_LEN];     /**< Module file path */
    char version[64];               /**< Module version */
    md_module_state_t state;        /**< Module state */
    
    /* Symbols */
    bool symbols_loaded;            /**< Whether symbols are loaded */
    char symbol_file[MD_MAX_PATH_LEN]; /**< Symbol file path */
    char symbol_status[64];         /**< Symbol load status */
    
    /* Address range */
    uint64_t base_address;          /**< Base address */
    uint64_t size;                  /**< Module size */
    
    /* Flags */
    bool is_optimized;              /**< Whether optimized */
    bool is_user_code;              /**< Whether user code */
    bool is_system;                 /**< Whether system library */
} md_module_t;

/* ============================================================================
 * Exception Info
 * ============================================================================ */

/**
 * @brief Unified exception information
 */
typedef struct md_exception {
    char type[256];                 /**< Exception type name */
    char message[MD_MAX_VARIABLE_STR_LEN]; /**< Exception message */
    char description[MD_MAX_VARIABLE_STR_LEN]; /**< Detailed description */
    char stack_trace[8192];         /**< Stack trace string */
    int thread_id;                  /**< Thread where exception occurred */
    int frame_id;                   /**< Frame where exception occurred */
    char source_path[MD_MAX_PATH_LEN]; /**< Source file */
    int line;                       /**< Line number */
    bool is_uncaught;               /**< Whether exception is uncaught */
} md_exception_t;

/* ============================================================================
 * Debug State
 * ============================================================================ */

/**
 * @brief Complete debug state snapshot
 * 
 * This structure represents a complete snapshot of the debug session state,
 * including execution state, threads, stack frames, breakpoints, etc.
 */
typedef struct md_debug_state {
    /* Execution State */
    md_exec_state_t exec_state;     /**< Current execution state */
    md_stop_reason_t stop_reason;   /**< Why program stopped (if stopped) */
    char stop_description[256];     /**< Human-readable stop description */
    
    /* Current Context */
    int current_thread_id;          /**< Currently focused thread */
    int current_frame_id;           /**< Currently focused stack frame */
    
    /* Program Info */
    char program_path[MD_MAX_PATH_LEN]; /**< Program being debugged */
    char working_dir[MD_MAX_PATH_LEN];  /**< Working directory */
    int pid;                        /**< Debuggee process ID */
    int exit_code;                  /**< Exit code (if terminated) */
    
    /* Exception */
    md_exception_t exception;       /**< Current exception (if any) */
    bool has_exception;             /**< Whether there's an active exception */
    
    /* Timestamps */
    uint64_t state_timestamp;       /**< State update timestamp (ms) */
    uint64_t last_stop_timestamp;   /**< Last stop time (ms) */
    uint64_t last_run_timestamp;    /**< Last run time (ms) */
    
    /* Statistics */
    uint64_t total_stops;           /**< Total number of stops */
    uint64_t total_breakpoint_hits; /**< Total breakpoint hits */
    uint64_t total_steps;           /**< Total steps */
} md_debug_state_t;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Initialize a breakpoint structure
 */
MD_API void md_breakpoint_init(md_breakpoint_t *bp);

/**
 * @brief Clear a breakpoint structure
 */
MD_API void md_breakpoint_clear(md_breakpoint_t *bp);

/**
 * @brief Initialize a thread structure
 */
MD_API void md_thread_init(md_thread_t *thread);

/**
 * @brief Initialize a stack frame structure
 */
MD_API void md_stack_frame_init(md_stack_frame_t *frame);

/**
 * @brief Initialize a variable structure
 */
MD_API void md_variable_init(md_variable_t *var);

/**
 * @brief Initialize a scope structure
 */
MD_API void md_scope_init(md_scope_t *scope);

/**
 * @brief Initialize a module structure
 */
MD_API void md_module_init(md_module_t *module);

/**
 * @brief Initialize an exception structure
 */
MD_API void md_exception_init(md_exception_t *exc);

/**
 * @brief Initialize a debug state structure
 */
MD_API void md_debug_state_init(md_debug_state_t *state);

/**
 * @brief Clear debug state (reset to initial values)
 */
MD_API void md_debug_state_clear(md_debug_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* MAGIC_DEBUGGER_STATE_TYPES_H */
