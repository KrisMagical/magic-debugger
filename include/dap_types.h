/**
 * @file dap_types.h
 * @brief DAP (Debug Adapter Protocol) Type Definitions
 * 
 * This file contains all type definitions for the Debug Adapter Protocol
 * as specified in https://microsoft.github.io/debug-adapter-protocol/specification
 */

#ifndef MAGIC_DEBUGGER_DAP_TYPES_H
#define MAGIC_DEBUGGER_DAP_TYPES_H

#include "types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * DAP Protocol Constants
 * ============================================================================ */

/** DAP protocol version */
#define DAP_PROTOCOL_VERSION "1.65.0"

/** Maximum message size (4MB) */
#define DAP_MAX_MESSAGE_SIZE (4 * 1024 * 1024)

/** Maximum number of arguments */
#define DAP_MAX_ARGS 256

/** Maximum variable reference depth */
#define DAP_MAX_VARIABLE_DEPTH 10

/* ============================================================================
 * Message Types
 * ============================================================================ */

/**
 * @brief DAP message types
 */
typedef enum {
    DAP_MSG_TYPE_REQUEST    = 0,    /**< Request message */
    DAP_MSG_TYPE_RESPONSE   = 1,    /**< Response message */
    DAP_MSG_TYPE_EVENT      = 2,    /**< Event message */
    DAP_MSG_TYPE_UNKNOWN    = 99,   /**< Unknown/invalid type */
} dap_msg_type_t;

/**
 * @brief Get string representation of message type
 */
MD_API const char* dap_msg_type_string(dap_msg_type_t type);

/* ============================================================================
 * Error Codes
 * ============================================================================ */

/**
 * @brief DAP-specific error codes
 */
typedef enum {
    DAP_ERROR_NONE                  = 0,
    DAP_ERROR_PARSE_FAILED          = -1,   /**< JSON parse failed */
    DAP_ERROR_INVALID_MESSAGE       = -2,   /**< Invalid message format */
    DAP_ERROR_MISSING_FIELD         = -3,   /**< Required field missing */
    DAP_ERROR_TYPE_MISMATCH         = -4,   /**< Field type mismatch */
    DAP_ERROR_BUFFER_TOO_SMALL      = -5,   /**< Buffer too small */
    DAP_ERROR_TIMEOUT               = -6,   /**< Operation timeout */
    DAP_ERROR_CONNECTION_CLOSED     = -7,   /**< Connection closed */
    DAP_ERROR_REQUEST_FAILED        = -8,   /**< Request failed */
    DAP_ERROR_NOT_SUPPORTED         = -9,   /**< Feature not supported */
    DAP_ERROR_INVALID_SEQ           = -10,  /**< Invalid sequence number */
    DAP_ERROR_CANCELED              = -11,  /**< Request canceled */
} dap_error_t;

/**
 * @brief Get DAP error description
 */
MD_API const char* dap_error_string(dap_error_t error);

/* ============================================================================
 * DAP Value Types
 * ============================================================================ */

/**
 * @brief Variable presentation hints
 */
typedef enum {
    DAP_PRESENTATION_HINT_NORMAL    = 0,
    DAP_PRESENTATION_HINT_VIRTUAL   = 1,
    DAP_PRESENTATION_HINT_INTERNAL  = 2,
} dap_presentation_hint_t;

/**
 * @brief Variable evaluate names
 */
typedef enum {
    DAP_VARIABLE_SCOPE_LOCAL    = 0,
    DAP_VARIABLE_SCOPE_GLOBAL   = 1,
    DAP_VARIABLE_SCOPE_CLOSURE  = 2,
} dap_variable_scope_t;

/* ============================================================================
 * DAP Structures
 * ============================================================================ */

/**
 * @brief Source location
 */
typedef struct dap_source {
    char *name;                 /**< Source name */
    char *path;                 /**< Source path */
    int source_reference;       /**< Source reference (for dynamically discovered sources) */
    char *presentation_hint;    /**< "normal", "emphasize", "deemphasize" */
    char *origin;               /**< Source origin */
    void *sources;              /**< Array of nested sources (optional) */
    int adapter_data;           /**< Adapter-specific data (opaque) */
} dap_source_t;

/**
 * @brief Source breakpoint
 */
typedef struct dap_source_breakpoint {
    int line;                   /**< Line number */
    int column;                 /**< Column number (optional, 0 if not set) */
    char *condition;            /**< Condition expression (optional) */
    char *hit_condition;        /**< Hit condition (optional) */
    char *log_message;          /**< Log message for logpoints (optional) */
} dap_source_breakpoint_t;

/**
 * @brief Breakpoint information
 */
typedef struct dap_breakpoint {
    int id;                     /**< Breakpoint ID */
    bool verified;              /**< Whether breakpoint is verified */
    char *message;              /**< Verification message */
    dap_source_t *source;       /**< Source (optional) */
    int line;                   /**< Line number */
    int column;                 /**< Column number */
    int end_line;               /**< End line (optional) */
    int end_column;             /**< End column (optional) */
    char *instruction_reference; /**< Instruction reference (optional) */
    int offset;                 /**< Offset (optional) */
} dap_breakpoint_t;

/**
 * @brief Stack frame
 */
typedef struct dap_stack_frame {
    int id;                     /**< Frame ID */
    char *name;                 /**< Frame name (function name) */
    dap_source_t *source;       /**< Source (optional) */
    int line;                   /**< Line number */
    int column;                 /**< Column number */
    int end_line;               /**< End line (optional) */
    int end_column;             /**< End column (optional) */
    char *instruction_pointer_reference; /**< Instruction pointer (optional) */
    int module_id;              /**< Module ID (optional) */
    char *presentation_hint;    /**< "normal", "label", "subtle" */
} dap_stack_frame_t;

/**
 * @brief Variable
 */
typedef struct dap_variable {
    char *name;                 /**< Variable name */
    char *value;                /**< Variable value string */
    char *type;                 /**< Variable type (optional) */
    char *presentation_hint;    /**< Presentation hint */
    char *evaluate_name;        /**< Evaluate name (for evaluation) */
    int variables_reference;    /**< Reference to child variables (0 if no children) */
    int named_variables;        /**< Number of named child variables */
    int indexed_variables;      /**< Number of indexed child variables */
    int memory_reference;       /**< Memory reference (optional) */
} dap_variable_t;

/**
 * @brief Scope (for variables)
 */
typedef struct dap_scope {
    char *name;                 /**< Scope name */
    char *presentation_hint;    /**< "arguments", "locals", "registers", etc. */
    int variables_reference;    /**< Reference to variables */
    int named_variables;        /**< Number of named variables */
    int indexed_variables;      /**< Number of indexed variables */
    bool expensive;             /**< Whether getting variables is expensive */
    dap_source_t *source;       /**< Source (optional) */
    int line;                   /**< Line (optional) */
    int column;                 /**< Column (optional) */
    int end_line;               /**< End line (optional) */
    int end_column;             /**< End column (optional) */
} dap_scope_t;

/**
 * @brief Thread information
 */
typedef struct dap_thread {
    int id;                     /**< Thread ID */
    char *name;                 /**< Thread name */
} dap_thread_t;

/**
 * @brief Module information
 */
typedef struct dap_module {
    int id;                     /**< Module ID (or string) */
    char *name;                 /**< Module name */
    char *path;                 /**< Module path (optional) */
    bool is_optimized;          /**< Whether optimized (optional) */
    bool is_user_code;          /**< Whether user code (optional) */
    char *version;              /**< Version (optional) */
    char *symbol_status;        /**< Symbol status (optional) */
    char *symbol_file_path;     /**< Symbol file path (optional) */
    char *date_time_stamp;      /**< Date time stamp (optional) */
    char *address_range;        /**< Address range (optional) */
} dap_module_t;

/* ============================================================================
 * Exception Information
 * ============================================================================ */

/**
 * @brief Exception details
 */
typedef struct dap_exception_details {
    char *message;              /**< Exception message */
    char *type_name;            /**< Exception type name */
    bool full_stack_trace;      /**< Whether full stack trace is available */
    char *stack_trace;          /**< Stack trace string */
    char *inner_exception;      /**< Inner exception (optional) */
    dap_source_t *source;       /**< Source location (optional) */
    int line;                   /**< Line number (optional) */
    int column;                 /**< Column number (optional) */
} dap_exception_details_t;

/* ============================================================================
 * Event Types
 * ============================================================================ */

/**
 * @brief Stopped event reasons
 */
typedef enum {
    DAP_STOP_REASON_STEP            = 0,
    DAP_STOP_REASON_BREAKPOINT      = 1,
    DAP_STOP_REASON_EXCEPTION       = 2,
    DAP_STOP_REASON_PAUSE           = 3,
    DAP_STOP_REASON_ENTRY           = 4,
    DAP_STOP_REASON_GOTO            = 5,
    DAP_STOP_REASON_FUNCTION        = 6,
    DAP_STOP_REASON_DATA_BREAKPOINT = 7,
    DAP_STOP_REASON_INSTRUCTION     = 8,
} dap_stop_reason_t;

/**
 * @brief Get string representation of stop reason
 */
MD_API const char* dap_stop_reason_string(dap_stop_reason_t reason);

/**
 * @brief Parse stop reason from string
 */
MD_API dap_stop_reason_t dap_stop_reason_from_string(const char *str);

/* ============================================================================
 * Message Structures
 * ============================================================================ */

/**
 * @brief DAP protocol message header
 */
typedef struct dap_message_header {
    int content_length;         /**< Content length in bytes */
    char *content_type;         /**< Content type (usually "application/vscode-jsonrpc") */
} dap_message_header_t;

/**
 * @brief DAP request message
 */
typedef struct dap_request {
    int seq;                    /**< Sequence number */
    char *command;              /**< Command name */
    void *arguments;            /**< Command arguments (JSON object) */
} dap_request_t;

/**
 * @brief DAP response message
 */
typedef struct dap_response {
    int request_seq;            /**< Request sequence number */
    bool success;               /**< Whether request succeeded */
    char *command;              /**< Command name */
    char *message;              /**< Error message (if failed) */
    void *body;                 /**< Response body (JSON object) */
    int error_code;             /**< Error code (if failed) */
} dap_response_t;

/**
 * @brief DAP event message
 */
typedef struct dap_event {
    int seq;                    /**< Sequence number */
    char *event;                /**< Event name */
    void *body;                 /**< Event body (JSON object) */
} dap_event_t;

/**
 * @brief Generic DAP message wrapper
 */
typedef struct dap_message {
    dap_msg_type_t type;        /**< Message type */
    int seq;                    /**< Sequence number */
    
    union {
        dap_request_t request;
        dap_response_t response;
        dap_event_t event;
    } u;
    
    char *raw_json;             /**< Raw JSON string (for debugging) */
} dap_message_t;

/* ============================================================================
 * Initialize Parameters
 * ============================================================================ */

/**
 * @brief Client capabilities
 */
typedef struct dap_client_capabilities {
    char *locale;                       /**< Locale (e.g., "en-us") */
    char *lines_start_at1_or_0;         /**< "1" or "0" */
    char *path_format;                  /**< "path" or "uri" */
    bool supports_variable_type;        /**< Supports variable type */
    bool supports_variable_paging;      /**< Supports variable paging */
    bool supports_run_in_terminal;      /**< Supports runInTerminal request */
    bool supports_memory_references;    /**< Supports memory references */
    bool supports_progress_reporting;   /**< Supports progress reporting */
    bool supports_invalidated_event;    /**< Supports invalidated event */
    bool supports_memory_event;         /**< Supports memory event */
} dap_client_capabilities_t;

/**
 * @brief Initialize request arguments
 */
typedef struct dap_initialize_args {
    char *client_id;                    /**< Client ID */
    char *client_name;                  /**< Client name */
    char *adapter_id;                   /**< Adapter ID */
    char *locale;                       /**< Locale */
    bool lines_start_at1;               /**< Lines start at 1 */
    char *path_format;                  /**< Path format */
    bool supports_variable_type;        /**< Supports variable type */
    bool supports_variable_paging;      /**< Supports variable paging */
    bool supports_run_in_terminal;      /**< Supports runInTerminal */
    bool supports_memory_references;    /**< Supports memory references */
    bool supports_progress_reporting;   /**< Supports progress reporting */
    bool supports_invalidated_event;    /**< Supports invalidated event */
    bool supports_memory_event;         /**< Supports memory event */
} dap_initialize_args_t;

/**
 * @brief Debugger capabilities (from initialize response)
 */
typedef struct dap_capabilities {
    bool supports_configuration_done_request;
    bool supports_function_breakpoints;
    bool supports_conditional_breakpoints;
    bool supports_hit_conditional_breakpoints;
    bool supports_evaluate_for_hovers;
    bool supports_step_back;
    bool supports_set_variable;
    bool supports_restart_frame;
    bool supports_goto_targets_request;
    bool supports_step_in_targets_request;
    bool supports_completions_request;
    bool supports_modules_request;
    bool supports_exception_options;
    bool supports_value_formatting_options;
    bool supports_exception_info_request;
    bool support_terminate_debuggee;
    bool supports_delayed_stack_trace_loading;
    bool supports_loaded_sources_request;
    bool supports_log_points;
    bool supports_terminate_threads_request;
    bool supports_set_expression;
    bool supports_terminate_request;
    bool supports_data_breakpoints;
    bool supports_read_memory_request;
    bool supports_disassemble_request;
    bool supports_cancel_request;
    bool supports_breakpoint_locations_request;
    bool supports_clipboard_context;
    bool supports_stepping_granularity;
    bool supports_instruction_breakpoints;
    bool supports_filtering_by_source;
    bool supports_single_thread_execution_requests;
    char *exception_breakpoint_filters; /**< JSON array string */
    char *additional_module_columns;    /**< JSON array string */
    char *supported_checksum_algorithms; /**< JSON array string */
} dap_capabilities_t;

/* ============================================================================
 * Launch/Attach Arguments
 * ============================================================================ */

/**
 * @brief Launch request arguments
 */
typedef struct dap_launch_args {
    bool no_debug;                      /**< No debug mode */
    bool stop_on_entry;                 /**< Stop on entry */
    char *program;                      /**< Program path */
    char *args;                         /**< Arguments (JSON array) */
    char *cwd;                          /**< Working directory */
    char *env;                          /**< Environment (JSON object) */
    char *additional_properties;        /**< Additional properties (JSON) */
} dap_launch_args_t;

/**
 * @brief Attach request arguments (debugger-specific)
 */
typedef struct dap_attach_args {
    int process_id;                     /**< Process ID to attach to */
    char *additional_properties;        /**< Additional properties (JSON) */
} dap_attach_args_t;

/* ============================================================================
 * Stepping Granularity
 * ============================================================================ */

/**
 * @brief Step granularity
 */
typedef enum {
    DAP_GRANULARITY_STATEMENT   = 0,
    DAP_GRANULARITY_LINE        = 1,
    DAP_GRANULARITY_INSTRUCTION = 2,
} dap_step_granularity_t;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Initialize client capabilities with defaults
 */
MD_API void dap_client_capabilities_init(dap_client_capabilities_t *caps);

/**
 * @brief Initialize capabilities structure
 */
MD_API void dap_capabilities_init(dap_capabilities_t *caps);

/**
 * @brief Initialize launch arguments
 */
MD_API void dap_launch_args_init(dap_launch_args_t *args);

/**
 * @brief Free source structure
 */
MD_API void dap_source_free(dap_source_t *source);

/**
 * @brief Free breakpoint structure
 */
MD_API void dap_breakpoint_free(dap_breakpoint_t *bp);

/**
 * @brief Free stack frame structure
 */
MD_API void dap_stack_frame_free(dap_stack_frame_t *frame);

/**
 * @brief Free variable structure
 */
MD_API void dap_variable_free(dap_variable_t *var);

/**
 * @brief Free scope structure
 */
MD_API void dap_scope_free(dap_scope_t *scope);

/**
 * @brief Free message structure
 */
MD_API void dap_message_free(dap_message_t *msg);

/**
 * @brief Free capabilities structure
 */
MD_API void dap_capabilities_free(dap_capabilities_t *caps);

#ifdef __cplusplus
}
#endif

#endif /* MAGIC_DEBUGGER_DAP_TYPES_H */
