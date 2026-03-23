/**
 * @file dap_client.h
 * @brief DAP Client API
 * 
 * This file defines the API for the DAP (Debug Adapter Protocol) client,
 * which handles communication with debug adapters like LLDB-DAP and GDB.
 */

#ifndef MAGIC_DEBUGGER_DAP_CLIENT_H
#define MAGIC_DEBUGGER_DAP_CLIENT_H

#include "types.h"
#include "dap_types.h"
#include "dap_protocol.h"
#include "session_manager.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Default timeout for I/O operations */
#define DAP_DEFAULT_TIMEOUT_MS   10000

/** Maximum pending requests */
#define DAP_MAX_PENDING          64

/** Maximum event handlers */
#define DAP_MAX_EVENT_HANDLERS   32

/** Default verbosity level */
#define DAP_DEFAULT_VERBOSITY    0

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

struct md_session;
struct md_io_buffer;

/* ============================================================================
 * Pending Request
 * ============================================================================ */

/**
 * @brief Pending request tracking
 */
typedef struct dap_pending_request {
    int seq;                        /**< Request sequence number */
    char *command;                  /**< Command name */
    bool completed;                 /**< Whether response received */
    bool success;                   /**< Whether request succeeded */
    cJSON *response_body;           /**< Response body (if success) */
    char *error_message;            /**< Error message (if failed) */
} dap_pending_request_t;

/* ============================================================================
 * Event Handler
 * ============================================================================ */

/**
 * @brief Event callback function type
 */
typedef void (*dap_event_callback_t)(dap_event_t *event, void *user_data);

/**
 * @brief Event handler registration
 */
typedef struct dap_event_handler {
    char *event_name;               /**< Event name to handle */
    dap_event_callback_t callback;  /**< Callback function */
    void *user_data;                /**< User data passed to callback */
} dap_event_handler_t;

/* ============================================================================
 * Client Configuration
 * ============================================================================ */

/**
 * @brief DAP client configuration
 */
typedef struct dap_client_config {
    int timeout_ms;                 /**< I/O timeout in milliseconds */
    int max_pending;                /**< Maximum pending requests */
    int max_event_handlers;         /**< Maximum event handlers */
    int verbosity;                  /**< Verbosity level (0-3) */
} dap_client_config_t;

/* ============================================================================
 * DAP Client Structure
 * ============================================================================ */

/**
 * @brief DAP client structure
 */
typedef struct dap_client {
    struct md_session *session;     /**< Underlying session */
    
    /* State */
    bool initialized;               /**< Whether initialize completed */
    bool received_initialized_event; /**< Whether initialized event received */
    int seq;                        /**< Current sequence number */
    md_debugger_type_t debugger_type; /**< Type of debugger */
    
    /* Capabilities */
    dap_capabilities_t capabilities; /**< Debugger capabilities */
    
    /* I/O */
    struct md_io_buffer *read_buffer; /**< Read buffer */
    int timeout_ms;                 /**< I/O timeout */
    
    /* Pending requests */
    dap_pending_request_t *pending_requests;
    int pending_count;
    int max_pending;
    
    /* Event handlers */
    dap_event_handler_t *event_handlers;
    int event_handler_count;
    int max_event_handlers;
    
    /* Statistics */
    uint64_t requests_sent;
    uint64_t responses_received;
    uint64_t events_received;
    
    /* Settings */
    int verbosity;
    void *user_data;
} dap_client_t;

/* ============================================================================
 * Subsystem Initialization
 * ============================================================================ */

/**
 * @brief Initialize DAP client subsystem
 * @return Error code
 */
MD_API md_error_t dap_client_init(void);

/**
 * @brief Shutdown DAP client subsystem
 */
MD_API void dap_client_shutdown(void);

/**
 * @brief Check if subsystem is initialized
 * @return true if initialized
 */
MD_API bool dap_client_is_subsystem_initialized(void);

/* ============================================================================
 * Client Creation/Destruction
 * ============================================================================ */

/**
 * @brief Create a DAP client
 * @param session Session to attach to
 * @param config Client configuration (NULL for defaults)
 * @return Client handle, or NULL on error
 */
MD_API dap_client_t* dap_client_create(struct md_session *session,
                                        const dap_client_config_t *config);

/**
 * @brief Destroy a DAP client
 * @param client Client handle
 */
MD_API void dap_client_destroy(dap_client_t *client);

/**
 * @brief Initialize client configuration with defaults
 * @param config Configuration to initialize
 */
MD_API void dap_client_config_init(dap_client_config_t *config);

/* ============================================================================
 * Message Sending
 * ============================================================================ */

/**
 * @brief Send raw JSON message
 * @param client Client handle
 * @param json_str JSON string to send
 * @param json_len Length of JSON string
 * @return Error code
 */
MD_API md_error_t dap_client_send_raw(dap_client_t *client, const char *json_str, size_t json_len);

/**
 * @brief Send a request
 * @param client Client handle
 * @param command Command name
 * @param args Arguments (cJSON object, can be NULL)
 * @param seq Sequence number
 * @return Error code
 */
MD_API md_error_t dap_client_send_request(dap_client_t *client, const char *command,
                                          const cJSON *args, int seq);

/**
 * @brief Send an event
 * @param client Client handle
 * @param event_name Event name
 * @param body Event body (cJSON object, can be NULL)
 * @return Error code
 */
MD_API md_error_t dap_client_send_event(dap_client_t *client, const char *event_name,
                                         const cJSON *body);

/* ============================================================================
 * Message Receiving
 * ============================================================================ */

/**
 * @brief Receive a message
 * @param client Client handle
 * @param message Pointer to store received message
 * @param timeout_ms Timeout in milliseconds
 * @return Error code
 */
MD_API md_error_t dap_client_receive(dap_client_t *client, cJSON **message, int timeout_ms);

/**
 * @brief Process pending messages
 * @param client Client handle
 * @param timeout_ms Timeout in milliseconds
 * @return Error code
 */
MD_API md_error_t dap_client_process_messages(dap_client_t *client, int timeout_ms);

/**
 * @brief Wait for a response
 * @param client Client handle
 * @param seq Sequence number to wait for
 * @param timeout_ms Timeout in milliseconds
 * @return Error code
 */
MD_API md_error_t dap_client_wait_for_response(dap_client_t *client, int seq, int timeout_ms);

/* ============================================================================
 * Event Handler Registration
 * ============================================================================ */

/**
 * @brief Register an event handler
 * @param client Client handle
 * @param event_name Event name to handle
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return Error code
 */
MD_API md_error_t dap_client_register_event_handler(dap_client_t *client, const char *event_name,
                                                     dap_event_callback_t callback, void *user_data);

/**
 * @brief Unregister an event handler
 * @param client Client handle
 * @param event_name Event name
 * @return Error code
 */
MD_API md_error_t dap_client_unregister_event_handler(dap_client_t *client, const char *event_name);

/**
 * @brief Check if event handler is registered
 * @param client Client handle
 * @param event_name Event name
 * @return true if registered
 */
MD_API bool dap_client_has_event_handler(dap_client_t *client, const char *event_name);

/* ============================================================================
 * Core Commands
 * ============================================================================ */

/**
 * @brief Initialize the debugger
 * @param client Client handle
 * @param params Initialize parameters (NULL for defaults)
 * @param out_caps Pointer to store capabilities (optional)
 * @return Error code
 */
MD_API md_error_t dap_client_initialize(dap_client_t *client, 
                                         const dap_initialize_args_t *params,
                                         dap_capabilities_t *out_caps);

/**
 * @brief Launch the program
 * @param client Client handle
 * @param params Launch parameters
 * @param success Pointer to store success flag (optional)
 * @return Error code
 */
MD_API md_error_t dap_client_launch(dap_client_t *client, const dap_launch_args_t *params,
                                     bool *success);

/**
 * @brief Set breakpoints
 * @param client Client handle
 * @param path Source file path
 * @param breakpoints Array of breakpoints
 * @param count Number of breakpoints
 * @param results Pointer to store results (optional)
 * @param result_count Pointer to store result count (optional)
 * @return Error code
 */
MD_API md_error_t dap_client_set_breakpoints(dap_client_t *client, const char *path,
                                              const dap_source_breakpoint_t *breakpoints,
                                              int count, dap_breakpoint_t **results,
                                              int *result_count);

/**
 * @brief Set breakpoints by line numbers
 * @param client Client handle
 * @param path Source file path
 * @param lines Array of line numbers
 * @param line_count Number of lines
 * @param results Pointer to store results (optional)
 * @param result_count Pointer to store result count (optional)
 * @return Error code
 */
MD_API md_error_t dap_client_set_breakpoints_file(dap_client_t *client, const char *path,
                                                   const int *lines, int line_count,
                                                   dap_breakpoint_t **results, int *result_count);

/**
 * @brief Signal configuration done
 * @param client Client handle
 * @return Error code
 */
MD_API md_error_t dap_client_configuration_done(dap_client_t *client);

/**
 * @brief Continue execution
 * @param client Client handle
 * @param thread_id Thread to continue
 * @param success Pointer to store success flag (optional)
 * @return Error code
 */
MD_API md_error_t dap_client_continue(dap_client_t *client, int thread_id, bool *success);

/**
 * @brief Step over
 * @param client Client handle
 * @param thread_id Thread to step
 * @return Error code
 */
MD_API md_error_t dap_client_next(dap_client_t *client, int thread_id);

/**
 * @brief Step into
 * @param client Client handle
 * @param thread_id Thread to step
 * @return Error code
 */
MD_API md_error_t dap_client_step_in(dap_client_t *client, int thread_id);

/**
 * @brief Step out
 * @param client Client handle
 * @param thread_id Thread to step
 * @return Error code
 */
MD_API md_error_t dap_client_step_out(dap_client_t *client, int thread_id);

/**
 * @brief Disconnect from debugger
 * @param client Client handle
 * @param terminate_debuggee Whether to terminate the debuggee
 * @return Error code
 */
MD_API md_error_t dap_client_disconnect(dap_client_t *client, bool terminate_debuggee);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Check if client is initialized
 * @param client Client handle
 * @return true if initialized
 */
MD_API bool dap_client_is_initialized(dap_client_t *client);

/**
 * @brief Get current sequence number
 * @param client Client handle
 * @return Sequence number
 */
MD_API int dap_client_get_seq(dap_client_t *client);

/**
 * @brief Get the underlying session
 * @param client Client handle
 * @return Session handle
 */
MD_API struct md_session* dap_client_get_session(dap_client_t *client);

/**
 * @brief Get debugger capabilities
 * @param client Client handle
 * @return Capabilities pointer
 */
MD_API const dap_capabilities_t* dap_client_get_capabilities(dap_client_t *client);

/**
 * @brief Set I/O timeout
 * @param client Client handle
 * @param timeout_ms Timeout in milliseconds (-1 for infinite)
 * @return Error code
 */
MD_API md_error_t dap_client_set_timeout(dap_client_t *client, int timeout_ms);

/**
 * @brief Get I/O timeout
 * @param client Client handle
 * @param timeout_ms Pointer to store timeout
 * @return Error code
 */
MD_API md_error_t dap_client_get_timeout(dap_client_t *client, int *timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* MAGIC_DEBUGGER_DAP_CLIENT_H */
