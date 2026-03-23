/**
 * @file state_model.h
 * @brief Unified Debug State Model API for Magic Debugger
 * 
 * The State Model provides a unified abstraction layer over different
 * debug adapters (LLDB, GDB, etc.), shielding the frontend from
 * debugger-specific differences.
 * 
 * Key Features:
 *   - Unified execution state management
 *   - Breakpoint state management with ID mapping
 *   - Thread and stack frame state tracking
 *   - Variable scope management
 *   - DAP event to state translation
 * 
 * Phase 3: State Model - Unified Debug State Representation
 */

#ifndef MAGIC_DEBUGGER_STATE_MODEL_H
#define MAGIC_DEBUGGER_STATE_MODEL_H

#include "types.h"
#include "state_types.h"
#include "dap_types.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

struct md_session;
struct dap_client;
struct md_state_model;

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Opaque state model handle
 */
typedef struct md_state_model md_state_model_t;

/* ============================================================================
 * State Change Callbacks
 * ============================================================================ */

/**
 * @brief State change event types
 */
typedef enum md_state_event {
    MD_STATE_EVENT_EXEC_CHANGED       = 0x0001,  /**< Execution state changed */
    MD_STATE_EVENT_THREAD_CHANGED     = 0x0002,  /**< Thread state changed */
    MD_STATE_EVENT_BREAKPOINT_CHANGED = 0x0004,  /**< Breakpoint state changed */
    MD_STATE_EVENT_STACK_CHANGED      = 0x0008,  /**< Stack frame changed */
    MD_STATE_EVENT_VARIABLE_CHANGED   = 0x0010,  /**< Variable changed */
    MD_STATE_EVENT_MODULE_CHANGED     = 0x0020,  /**< Module state changed */
    MD_STATE_EVENT_EXCEPTION_CHANGED  = 0x0040,  /**< Exception state changed */
    MD_STATE_EVENT_ALL_CHANGED        = 0x00FF,  /**< All state changed */
} md_state_event_t;

/**
 * @brief State change callback function type
 * @param model State model that changed
 * @param event Type of state change
 * @param data Event-specific data (may be NULL)
 * @param user_data User-provided callback data
 */
typedef void (*md_state_callback_t)(md_state_model_t *model, 
                                     md_state_event_t event,
                                     void *data,
                                     void *user_data);

/**
 * @brief State change listener registration
 */
typedef struct md_state_listener {
    md_state_event_t events;            /**< Events to listen for */
    md_state_callback_t callback;       /**< Callback function */
    void *user_data;                    /**< User data for callback */
} md_state_listener_t;

/* ============================================================================
 * State Model Configuration
 * ============================================================================ */

/**
 * @brief State model configuration
 */
typedef struct md_state_config {
    int max_breakpoints;                /**< Maximum breakpoints */
    int max_threads;                    /**< Maximum threads */
    int max_stack_frames;               /**< Maximum stack frames */
    int max_variables;                  /**< Maximum variables */
    int max_modules;                    /**< Maximum modules */
    bool auto_update_threads;           /**< Auto-update thread state */
    bool auto_update_stack;             /**< Auto-update stack on stop */
    bool auto_update_variables;         /**< Auto-update variables on stop */
} md_state_config_t;

/* ============================================================================
 * State Model Lifecycle
 * ============================================================================ */

/**
 * @brief Create a new state model
 * @param config Configuration (NULL for defaults)
 * @return State model handle, or NULL on error
 */
MD_API md_state_model_t* md_state_model_create(const md_state_config_t *config);

/**
 * @brief Destroy a state model
 * @param model State model handle
 */
MD_API void md_state_model_destroy(md_state_model_t *model);

/**
 * @brief Reset state model to initial state
 * @param model State model handle
 * @return Error code
 */
MD_API md_error_t md_state_model_reset(md_state_model_t *model);

/**
 * @brief Initialize configuration with defaults
 * @param config Configuration to initialize
 */
MD_API void md_state_config_init(md_state_config_t *config);

/* ============================================================================
 * State Change Notification
 * ============================================================================ */

/**
 * @brief Register a state change listener
 * @param model State model handle
 * @param listener Listener to register
 * @return Error code
 */
MD_API md_error_t md_state_model_add_listener(md_state_model_t *model,
                                               const md_state_listener_t *listener);

/**
 * @brief Remove a state change listener
 * @param model State model handle
 * @param callback Callback function to remove
 * @return Error code
 */
MD_API md_error_t md_state_model_remove_listener(md_state_model_t *model,
                                                  md_state_callback_t callback);

/**
 * @brief Notify listeners of state change
 * @param model State model handle
 * @param event Event type
 * @param data Event data
 */
MD_API void md_state_model_notify(md_state_model_t *model,
                                   md_state_event_t event,
                                   void *data);

/* ============================================================================
 * Execution State Management
 * ============================================================================ */

/**
 * @brief Get current execution state
 * @param model State model handle
 * @return Execution state
 */
MD_API md_exec_state_t md_state_get_exec_state(md_state_model_t *model);

/**
 * @brief Set execution state
 * @param model State model handle
 * @param state New execution state
 * @return Error code
 */
MD_API md_error_t md_state_set_exec_state(md_state_model_t *model,
                                           md_exec_state_t state);

/**
 * @brief Get stop reason
 * @param model State model handle
 * @return Stop reason
 */
MD_API md_stop_reason_t md_state_get_stop_reason(md_state_model_t *model);

/**
 * @brief Set stop reason
 * @param model State model handle
 * @param reason Stop reason
 * @param description Human-readable description (optional)
 * @return Error code
 */
MD_API md_error_t md_state_set_stop_reason(md_state_model_t *model,
                                            md_stop_reason_t reason,
                                            const char *description);

/**
 * @brief Get current thread ID
 * @param model State model handle
 * @return Current thread ID, or MD_INVALID_ID
 */
MD_API int md_state_get_current_thread_id(md_state_model_t *model);

/**
 * @brief Set current thread ID
 * @param model State model handle
 * @param thread_id Thread ID
 * @return Error code
 */
MD_API md_error_t md_state_set_current_thread_id(md_state_model_t *model, int thread_id);

/**
 * @brief Get current frame ID
 * @param model State model handle
 * @return Current frame ID, or MD_INVALID_ID
 */
MD_API int md_state_get_current_frame_id(md_state_model_t *model);

/**
 * @brief Set current frame ID
 * @param model State model handle
 * @param frame_id Frame ID
 * @return Error code
 */
MD_API md_error_t md_state_set_current_frame_id(md_state_model_t *model, int frame_id);

/**
 * @brief Get complete debug state snapshot
 * @param model State model handle
 * @param state Pointer to store state
 * @return Error code
 */
MD_API md_error_t md_state_get_debug_state(md_state_model_t *model,
                                            md_debug_state_t *state);

/* ============================================================================
 * Breakpoint Management
 * ============================================================================ */

/**
 * @brief Add a breakpoint
 * @param model State model handle
 * @param bp Breakpoint to add (id will be assigned)
 * @return Internal breakpoint ID, or negative error code
 */
MD_API int md_state_add_breakpoint(md_state_model_t *model, md_breakpoint_t *bp);

/**
 * @brief Update a breakpoint
 * @param model State model handle
 * @param bp Breakpoint with updated data
 * @return Error code
 */
MD_API md_error_t md_state_update_breakpoint(md_state_model_t *model,
                                              const md_breakpoint_t *bp);

/**
 * @brief Remove a breakpoint by internal ID
 * @param model State model handle
 * @param id Internal breakpoint ID
 * @return Error code
 */
MD_API md_error_t md_state_remove_breakpoint(md_state_model_t *model, int id);

/**
 * @brief Get breakpoint by internal ID
 * @param model State model handle
 * @param id Internal breakpoint ID
 * @param bp Pointer to store breakpoint data
 * @return Error code
 */
MD_API md_error_t md_state_get_breakpoint(md_state_model_t *model, int id,
                                           md_breakpoint_t *bp);

/**
 * @brief Find breakpoint by DAP ID
 * @param model State model handle
 * @param dap_id DAP breakpoint ID
 * @param bp Pointer to store breakpoint data
 * @return Error code
 */
MD_API md_error_t md_state_find_breakpoint_by_dap_id(md_state_model_t *model,
                                                      int dap_id,
                                                      md_breakpoint_t *bp);

/**
 * @brief Find breakpoint by file and line
 * @param model State model handle
 * @param path Source file path
 * @param line Line number
 * @param bp Pointer to store breakpoint data
 * @return Error code
 */
MD_API md_error_t md_state_find_breakpoint_by_location(md_state_model_t *model,
                                                        const char *path,
                                                        int line,
                                                        md_breakpoint_t *bp);

/**
 * @brief Get all breakpoints
 * @param model State model handle
 * @param breakpoints Array to store breakpoints
 * @param max_count Maximum number of breakpoints to store
 * @param actual_count Pointer to store actual count
 * @return Error code
 */
MD_API md_error_t md_state_get_all_breakpoints(md_state_model_t *model,
                                                md_breakpoint_t *breakpoints,
                                                int max_count,
                                                int *actual_count);

/**
 * @brief Get breakpoint count
 * @param model State model handle
 * @return Number of breakpoints
 */
MD_API int md_state_get_breakpoint_count(md_state_model_t *model);

/**
 * @brief Clear all breakpoints
 * @param model State model handle
 * @return Error code
 */
MD_API md_error_t md_state_clear_all_breakpoints(md_state_model_t *model);

/**
 * @brief Enable/disable a breakpoint
 * @param model State model handle
 * @param id Breakpoint ID
 * @param enabled Enable state
 * @return Error code
 */
MD_API md_error_t md_state_set_breakpoint_enabled(md_state_model_t *model,
                                                   int id, bool enabled);

/* ============================================================================
 * Thread Management
 * ============================================================================ */

/**
 * @brief Add a thread
 * @param model State model handle
 * @param thread Thread to add
 * @return Error code
 */
MD_API md_error_t md_state_add_thread(md_state_model_t *model,
                                       const md_thread_t *thread);

/**
 * @brief Update a thread
 * @param model State model handle
 * @param thread Thread with updated data
 * @return Error code
 */
MD_API md_error_t md_state_update_thread(md_state_model_t *model,
                                          const md_thread_t *thread);

/**
 * @brief Remove a thread
 * @param model State model handle
 * @param thread_id Thread ID to remove
 * @return Error code
 */
MD_API md_error_t md_state_remove_thread(md_state_model_t *model, int thread_id);

/**
 * @brief Get thread by ID
 * @param model State model handle
 * @param thread_id Thread ID
 * @param thread Pointer to store thread data
 * @return Error code
 */
MD_API md_error_t md_state_get_thread(md_state_model_t *model, int thread_id,
                                       md_thread_t *thread);

/**
 * @brief Get all threads
 * @param model State model handle
 * @param threads Array to store threads
 * @param max_count Maximum number of threads to store
 * @param actual_count Pointer to store actual count
 * @return Error code
 */
MD_API md_error_t md_state_get_all_threads(md_state_model_t *model,
                                            md_thread_t *threads,
                                            int max_count,
                                            int *actual_count);

/**
 * @brief Get thread count
 * @param model State model handle
 * @return Number of threads
 */
MD_API int md_state_get_thread_count(md_state_model_t *model);

/**
 * @brief Clear all threads
 * @param model State model handle
 * @return Error code
 */
MD_API md_error_t md_state_clear_all_threads(md_state_model_t *model);

/* ============================================================================
 * Stack Frame Management
 * ============================================================================ */

/**
 * @brief Set stack frames for a thread
 * @param model State model handle
 * @param thread_id Thread ID
 * @param frames Array of stack frames
 * @param count Number of frames
 * @return Error code
 */
MD_API md_error_t md_state_set_stack_frames(md_state_model_t *model,
                                             int thread_id,
                                             const md_stack_frame_t *frames,
                                             int count);

/**
 * @brief Get stack frames for a thread
 * @param model State model handle
 * @param thread_id Thread ID
 * @param frames Array to store frames
 * @param max_count Maximum frames to store
 * @param actual_count Pointer to store actual count
 * @return Error code
 */
MD_API md_error_t md_state_get_stack_frames(md_state_model_t *model,
                                             int thread_id,
                                             md_stack_frame_t *frames,
                                             int max_count,
                                             int *actual_count);

/**
 * @brief Get a single stack frame
 * @param model State model handle
 * @param frame_id Frame ID
 * @param frame Pointer to store frame data
 * @return Error code
 */
MD_API md_error_t md_state_get_stack_frame(md_state_model_t *model,
                                            int frame_id,
                                            md_stack_frame_t *frame);

/**
 * @brief Get stack frame count for a thread
 * @param model State model handle
 * @param thread_id Thread ID
 * @return Number of stack frames
 */
MD_API int md_state_get_stack_frame_count(md_state_model_t *model, int thread_id);

/**
 * @brief Clear stack frames for a thread
 * @param model State model handle
 * @param thread_id Thread ID
 * @return Error code
 */
MD_API md_error_t md_state_clear_stack_frames(md_state_model_t *model, int thread_id);

/**
 * @brief Clear all stack frames
 * @param model State model handle
 * @return Error code
 */
MD_API md_error_t md_state_clear_all_stack_frames(md_state_model_t *model);

/* ============================================================================
 * Variable/Scope Management
 * ============================================================================ */

/**
 * @brief Set scopes for a stack frame
 * @param model State model handle
 * @param frame_id Frame ID
 * @param scopes Array of scopes
 * @param count Number of scopes
 * @return Error code
 */
MD_API md_error_t md_state_set_scopes(md_state_model_t *model,
                                       int frame_id,
                                       const md_scope_t *scopes,
                                       int count);

/**
 * @brief Get scopes for a stack frame
 * @param model State model handle
 * @param frame_id Frame ID
 * @param scopes Array to store scopes
 * @param max_count Maximum scopes to store
 * @param actual_count Pointer to store actual count
 * @return Error code
 */
MD_API md_error_t md_state_get_scopes(md_state_model_t *model,
                                       int frame_id,
                                       md_scope_t *scopes,
                                       int max_count,
                                       int *actual_count);

/**
 * @brief Set variables for a scope
 * @param model State model handle
 * @param scope_id Scope ID (variables reference)
 * @param variables Array of variables
 * @param count Number of variables
 * @return Error code
 */
MD_API md_error_t md_state_set_variables(md_state_model_t *model,
                                          int scope_id,
                                          const md_variable_t *variables,
                                          int count);

/**
 * @brief Get variables for a scope
 * @param model State model handle
 * @param scope_id Scope ID (variables reference)
 * @param variables Array to store variables
 * @param max_count Maximum variables to store
 * @param actual_count Pointer to store actual count
 * @return Error code
 */
MD_API md_error_t md_state_get_variables(md_state_model_t *model,
                                          int scope_id,
                                          md_variable_t *variables,
                                          int max_count,
                                          int *actual_count);

/**
 * @brief Get a single variable by ID
 * @param model State model handle
 * @param var_id Variable ID
 * @param variable Pointer to store variable data
 * @return Error code
 */
MD_API md_error_t md_state_get_variable(md_state_model_t *model,
                                         int var_id,
                                         md_variable_t *variable);

/**
 * @brief Clear all variables
 * @param model State model handle
 * @return Error code
 */
MD_API md_error_t md_state_clear_all_variables(md_state_model_t *model);

/* ============================================================================
 * Module Management
 * ============================================================================ */

/**
 * @brief Add a module
 * @param model State model handle
 * @param module Module to add
 * @return Error code
 */
MD_API md_error_t md_state_add_module(md_state_model_t *model,
                                       const md_module_t *module);

/**
 * @brief Update a module
 * @param model State model handle
 * @param module Module with updated data
 * @return Error code
 */
MD_API md_error_t md_state_update_module(md_state_model_t *model,
                                          const md_module_t *module);

/**
 * @brief Remove a module
 * @param model State model handle
 * @param module_id Module ID
 * @return Error code
 */
MD_API md_error_t md_state_remove_module(md_state_model_t *model, int module_id);

/**
 * @brief Get module by ID
 * @param model State model handle
 * @param module_id Module ID
 * @param module Pointer to store module data
 * @return Error code
 */
MD_API md_error_t md_state_get_module(md_state_model_t *model, int module_id,
                                       md_module_t *module);

/**
 * @brief Get all modules
 * @param model State model handle
 * @param modules Array to store modules
 * @param max_count Maximum modules to store
 * @param actual_count Pointer to store actual count
 * @return Error code
 */
MD_API md_error_t md_state_get_all_modules(md_state_model_t *model,
                                            md_module_t *modules,
                                            int max_count,
                                            int *actual_count);

/**
 * @brief Get module count
 * @param model State model handle
 * @return Number of modules
 */
MD_API int md_state_get_module_count(md_state_model_t *model);

/* ============================================================================
 * Exception Management
 * ============================================================================ */

/**
 * @brief Set exception information
 * @param model State model handle
 * @param exception Exception information
 * @return Error code
 */
MD_API md_error_t md_state_set_exception(md_state_model_t *model,
                                          const md_exception_t *exception);

/**
 * @brief Get exception information
 * @param model State model handle
 * @param exception Pointer to store exception data
 * @return Error code
 */
MD_API md_error_t md_state_get_exception(md_state_model_t *model,
                                          md_exception_t *exception);

/**
 * @brief Clear exception
 * @param model State model handle
 * @return Error code
 */
MD_API md_error_t md_state_clear_exception(md_state_model_t *model);

/**
 * @brief Check if there's an active exception
 * @param model State model handle
 * @return true if active exception
 */
MD_API bool md_state_has_exception(md_state_model_t *model);

/* ============================================================================
 * DAP Event Translation
 * ============================================================================ */

/**
 * @brief Handle DAP stopped event
 * @param model State model handle
 * @param body Event body (JSON)
 * @return Error code
 */
MD_API md_error_t md_state_handle_stopped_event(md_state_model_t *model,
                                                 const cJSON *body);

/**
 * @brief Handle DAP continued event
 * @param model State model handle
 * @param body Event body (JSON)
 * @return Error code
 */
MD_API md_error_t md_state_handle_continued_event(md_state_model_t *model,
                                                   const cJSON *body);

/**
 * @brief Handle DAP exited event
 * @param model State model handle
 * @param body Event body (JSON)
 * @return Error code
 */
MD_API md_error_t md_state_handle_exited_event(md_state_model_t *model,
                                                const cJSON *body);

/**
 * @brief Handle DAP terminated event
 * @param model State model handle
 * @param body Event body (JSON)
 * @return Error code
 */
MD_API md_error_t md_state_handle_terminated_event(md_state_model_t *model,
                                                    const cJSON *body);

/**
 * @brief Handle DAP thread event
 * @param model State model handle
 * @param body Event body (JSON)
 * @return Error code
 */
MD_API md_error_t md_state_handle_thread_event(md_state_model_t *model,
                                                const cJSON *body);

/**
 * @brief Handle DAP breakpoint event
 * @param model State model handle
 * @param body Event body (JSON)
 * @return Error code
 */
MD_API md_error_t md_state_handle_breakpoint_event(md_state_model_t *model,
                                                    const cJSON *body);

/**
 * @brief Handle DAP module event
 * @param model State model handle
 * @param body Event body (JSON)
 * @return Error code
 */
MD_API md_error_t md_state_handle_module_event(md_state_model_t *model,
                                                const cJSON *body);

/**
 * @brief Handle DAP output event
 * @param model State model handle
 * @param body Event body (JSON)
 * @param callback Output callback (receives output text)
 * @param user_data User data for callback
 * @return Error code
 */
MD_API md_error_t md_state_handle_output_event(md_state_model_t *model,
                                                const cJSON *body,
                                                void (*callback)(const char *category,
                                                                  const char *output,
                                                                  void *user_data),
                                                void *user_data);

/* ============================================================================
 * DAP Response Parsing
 * ============================================================================ */

/**
 * @brief Parse threads from DAP response
 * @param model State model handle
 * @param body Response body (JSON)
 * @return Error code
 */
MD_API md_error_t md_state_parse_threads_response(md_state_model_t *model,
                                                   const cJSON *body);

/**
 * @brief Parse stack frames from DAP response
 * @param model State model handle
 * @param thread_id Thread ID
 * @param body Response body (JSON)
 * @return Error code
 */
MD_API md_error_t md_state_parse_stacktrace_response(md_state_model_t *model,
                                                      int thread_id,
                                                      const cJSON *body);

/**
 * @brief Parse scopes from DAP response
 * @param model State model handle
 * @param frame_id Frame ID
 * @param body Response body (JSON)
 * @return Error code
 */
MD_API md_error_t md_state_parse_scopes_response(md_state_model_t *model,
                                                  int frame_id,
                                                  const cJSON *body);

/**
 * @brief Parse variables from DAP response
 * @param model State model handle
 * @param scope_id Scope ID (variables reference)
 * @param body Response body (JSON)
 * @return Error code
 */
MD_API md_error_t md_state_parse_variables_response(md_state_model_t *model,
                                                     int scope_id,
                                                     const cJSON *body);

/**
 * @brief Parse setBreakpoints response
 * @param model State model handle
 * @param path Source file path
 * @param breakpoints Requested breakpoints
 * @param bp_count Number of breakpoints requested
 * @param body Response body (JSON)
 * @return Error code
 */
MD_API md_error_t md_state_parse_set_breakpoints_response(md_state_model_t *model,
                                                           const char *path,
                                                           const dap_source_breakpoint_t *breakpoints,
                                                           int bp_count,
                                                           const cJSON *body);

/**
 * @brief Parse initialize response (capabilities)
 * @param model State model handle
 * @param body Response body (JSON)
 * @return Error code
 */
MD_API md_error_t md_state_parse_initialize_response(md_state_model_t *model,
                                                      const cJSON *body);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get timestamp in milliseconds
 * @return Current timestamp
 */
MD_API uint64_t md_state_get_timestamp_ms(void);

/**
 * @brief Dump state to string (for debugging)
 * @param model State model handle
 * @param buffer Buffer to store dump
 * @param size Buffer size
 * @return Error code
 */
MD_API md_error_t md_state_dump(md_state_model_t *model, char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* MAGIC_DEBUGGER_STATE_MODEL_H */
