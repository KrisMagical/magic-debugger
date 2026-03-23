/**
 * @file session_manager.h
 * @brief Session Manager for Magic Debugger
 * 
 * The Session Manager is responsible for managing the lifecycle of debug
 * sessions, including process creation, communication setup, and cleanup.
 * 
 * Phase 1 Implementation:
 *   - Process creation (fork + exec)
 *   - Pipe communication (stdin/stdout redirection)
 *   - Non-blocking I/O
 *   - Session lifecycle management
 */

#ifndef MAGIC_DEBUGGER_SESSION_MANAGER_H
#define MAGIC_DEBUGGER_SESSION_MANAGER_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

struct md_session;
struct md_session_manager;

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Opaque debug session handle
 */
typedef struct md_session md_session_t;

/**
 * @brief Opaque session manager handle
 */
typedef struct md_session_manager md_session_manager_t;

/* ============================================================================
 * Session Configuration
 * ============================================================================ */

/**
 * @brief Configuration for creating a debug session
 */
typedef struct {
    char debugger_path[MD_MAX_PATH_LEN];        /**< Path to debugger executable */
    char program_path[MD_MAX_PATH_LEN];         /**< Path to program to debug */
    char working_dir[MD_MAX_PATH_LEN];          /**< Working directory (optional) */
    char *args[MD_MAX_ARGS];                    /**< Additional arguments */
    int arg_count;                              /**< Number of additional arguments */
    md_debugger_type_t debugger_type;           /**< Type of debugger */
    bool redirect_stderr;                       /**< Whether to redirect stderr */
    bool create_pty;                            /**< Whether to create a pseudo-terminal */
} md_session_config_t;

/* ============================================================================
 * Session Information
 * ============================================================================ */

/**
 * @brief Information about a debug session
 */
typedef struct {
    int session_id;                             /**< Unique session identifier */
    int pid;                                    /**< Process ID of debugger */
    int stdin_fd;                               /**< File descriptor for stdin */
    int stdout_fd;                              /**< File descriptor for stdout */
    int stderr_fd;                              /**< File descriptor for stderr (if redirected) */
    md_session_state_t state;                   /**< Current session state */
    md_debugger_type_t debugger_type;           /**< Type of debugger */
    char debugger_name[MD_MAX_DEBUGGER_NAME];   /**< Name of debugger */
    char program_path[MD_MAX_PATH_LEN];         /**< Program being debugged */
    int exit_code;                              /**< Exit code (if terminated) */
    bool is_valid;                              /**< Whether this info is valid */
} md_session_info_t;

/* ============================================================================
 * Session Statistics
 * ============================================================================ */

/**
 * @brief Statistics for a debug session
 */
typedef struct {
    uint64_t bytes_sent;        /**< Total bytes sent to debugger */
    uint64_t bytes_received;    /**< Total bytes received from debugger */
    uint64_t messages_sent;     /**< Number of messages sent */
    uint64_t messages_received; /**< Number of messages received */
    uint64_t uptime_ms;         /**< Session uptime in milliseconds */
    int restart_count;          /**< Number of times session was restarted */
} md_session_stats_t;

/* ============================================================================
 * Session Manager Configuration
 * ============================================================================ */

/**
 * @brief Configuration for the session manager
 */
typedef struct {
    int max_sessions;           /**< Maximum concurrent sessions */
    int io_timeout_ms;          /**< I/O timeout in milliseconds */
    bool auto_cleanup;          /**< Auto-cleanup terminated sessions */
    md_exit_callback_t on_exit; /**< Callback for process exit */
    void *callback_data;        /**< User data for callbacks */
} md_manager_config_t;

/* ============================================================================
 * Session Manager API
 * ============================================================================ */

/**
 * @brief Initialize the session manager
 * @param config Configuration for the manager (NULL for defaults)
 * @return Session manager handle, or NULL on failure
 */
MD_API md_session_manager_t* md_manager_create(const md_manager_config_t *config);

/**
 * @brief Destroy the session manager
 * @param manager Session manager handle
 * 
 * This will terminate all active sessions and free all resources.
 */
MD_API void md_manager_destroy(md_session_manager_t *manager);

/**
 * @brief Get the number of active sessions
 * @param manager Session manager handle
 * @return Number of active sessions, or -1 on error
 */
MD_API int md_manager_get_session_count(md_session_manager_t *manager);

/**
 * @brief Get list of active session IDs
 * @param manager Session manager handle
 * @param ids Array to store session IDs
 * @param max_count Maximum number of IDs to store
 * @return Number of sessions, or negative error code
 */
MD_API int md_manager_get_session_ids(md_session_manager_t *manager,
                                       int *ids, int max_count);

/**
 * @brief Check if a session exists
 * @param manager Session manager handle
 * @param session_id Session ID to check
 * @return true if session exists, false otherwise
 */
MD_API bool md_manager_session_exists(md_session_manager_t *manager, int session_id);

/* ============================================================================
 * Session Creation and Destruction
 * ============================================================================ */

/**
 * @brief Create a new debug session
 * @param manager Session manager handle
 * @param config Session configuration
 * @return Session handle, or NULL on failure
 * 
 * This function:
 * 1. Creates pipes for stdin/stdout (and optionally stderr)
 * 2. Forks the process
 * 3. Execs the debugger
 * 4. Sets up non-blocking I/O
 */
MD_API md_session_t* md_session_create(md_session_manager_t *manager,
                                        const md_session_config_t *config);

/**
 * @brief Destroy a debug session
 * @param session Session handle
 * @return Error code
 * 
 * This will:
 * 1. Close all file descriptors
 * 2. Terminate the debugger process if still running
 * 3. Free all resources
 */
MD_API md_error_t md_session_destroy(md_session_t *session);

/**
 * @brief Get the session ID
 * @param session Session handle
 * @return Session ID, or -1 on error
 */
MD_API int md_session_get_id(md_session_t *session);

/**
 * @brief Get session information
 * @param session Session handle
 * @param info Pointer to store session info
 * @return Error code
 */
MD_API md_error_t md_session_get_info(md_session_t *session, md_session_info_t *info);

/**
 * @brief Get session statistics
 * @param session Session handle
 * @param stats Pointer to store statistics
 * @return Error code
 */
MD_API md_error_t md_session_get_stats(md_session_t *session, md_session_stats_t *stats);

/**
 * @brief Get the debugger type for a session
 * @param session Session handle
 * @return Debugger type
 */
MD_API md_debugger_type_t md_session_get_debugger_type(md_session_t *session);

/* ============================================================================
 * Process Control
 * ============================================================================ */

/**
 * @brief Terminate the debugger process
 * @param session Session handle
 * @param force If true, send SIGKILL; otherwise send SIGTERM
 * @return Error code
 */
MD_API md_error_t md_session_terminate(md_session_t *session, bool force);

/**
 * @brief Wait for the debugger process to exit
 * @param session Session handle
 * @param timeout_ms Timeout in milliseconds (-1 for infinite)
 * @param exit_code Pointer to store exit code (optional)
 * @return Error code
 */
MD_API md_error_t md_session_wait(md_session_t *session, int timeout_ms, int *exit_code);

/**
 * @brief Check if the debugger process is still running
 * @param session Session handle
 * @return true if running, false otherwise
 */
MD_API bool md_session_is_running(md_session_t *session);

/**
 * @brief Get the session state
 * @param session Session handle
 * @return Session state, or MD_SESSION_STATE_NONE on error
 */
MD_API md_session_state_t md_session_get_state(md_session_t *session);

/* ============================================================================
 * Communication
 * ============================================================================ */

/**
 * @brief Send data to the debugger's stdin
 * @param session Session handle
 * @param data Data to send
 * @param size Size of data in bytes
 * @param bytes_sent Pointer to store number of bytes sent (optional)
 * @return Error code
 */
MD_API md_error_t md_session_send(md_session_t *session, const void *data,
                                   size_t size, size_t *bytes_sent);

/**
 * @brief Send a null-terminated string to the debugger
 * @param session Session handle
 * @param str String to send
 * @param bytes_sent Pointer to store number of bytes sent (optional)
 * @return Error code
 */
MD_API md_error_t md_session_send_string(md_session_t *session, const char *str,
                                          size_t *bytes_sent);

/**
 * @brief Receive data from the debugger's stdout
 * @param session Session handle
 * @param buffer Buffer to store received data
 * @param size Size of buffer in bytes
 * @param bytes_received Pointer to store number of bytes received (optional)
 * @return Error code
 * 
 * This is a non-blocking call. If no data is available, it will return
 * MD_SUCCESS with *bytes_received = 0.
 */
MD_API md_error_t md_session_receive(md_session_t *session, void *buffer,
                                      size_t size, size_t *bytes_received);

/**
 * @brief Receive a line from the debugger's stdout
 * @param session Session handle
 * @param buffer Buffer to store the line
 * @param size Size of buffer in bytes
 * @param line_len Pointer to store line length (optional)
 * @return Error code
 * 
 * Reads until a newline character or buffer is full. The newline is
 * included in the output.
 */
MD_API md_error_t md_session_receive_line(md_session_t *session, char *buffer,
                                           size_t size, size_t *line_len);

/**
 * @brief Read from stderr (if redirected)
 * @param session Session handle
 * @param buffer Buffer to store data
 * @param size Size of buffer
 * @param bytes_read Pointer to store bytes read (optional)
 * @return Error code
 */
MD_API md_error_t md_session_read_stderr(md_session_t *session, void *buffer,
                                          size_t size, size_t *bytes_read);

/* ============================================================================
 * File Descriptor Access
 * ============================================================================ */

/**
 * @brief Get the stdin file descriptor
 * @param session Session handle
 * @return File descriptor, or -1 on error
 */
MD_API int md_session_get_stdin_fd(md_session_t *session);

/**
 * @brief Get the stdout file descriptor
 * @param session Session handle
 * @return File descriptor, or -1 on error
 */
MD_API int md_session_get_stdout_fd(md_session_t *session);

/**
 * @brief Get the stderr file descriptor
 * @param session Session handle
 * @return File descriptor, or -1 if stderr is not redirected
 */
MD_API int md_session_get_stderr_fd(md_session_t *session);

/**
 * @brief Get the process ID
 * @param session Session handle
 * @return Process ID, or -1 on error
 */
MD_API int md_session_get_pid(md_session_t *session);

/* ============================================================================
 * Configuration Helpers
 * ============================================================================ */

/**
 * @brief Initialize a session config with defaults
 * @param config Config to initialize
 */
MD_API void md_session_config_init(md_session_config_t *config);

/**
 * @brief Set the debugger path in config
 * @param config Config to modify
 * @param path Path to debugger
 * @return Error code
 */
MD_API md_error_t md_session_config_set_debugger(md_session_config_t *config,
                                                  const char *path);

/**
 * @brief Set the program path in config
 * @param config Config to modify
 * @param path Path to program
 * @return Error code
 */
MD_API md_error_t md_session_config_set_program(md_session_config_t *config,
                                                 const char *path);

/**
 * @brief Add an argument to the config
 * @param config Config to modify
 * @param arg Argument to add
 * @return Error code
 */
MD_API md_error_t md_session_config_add_arg(md_session_config_t *config,
                                             const char *arg);

/**
 * @brief Clear all arguments from config
 * @param config Config to modify
 */
MD_API void md_session_config_clear_args(md_session_config_t *config);

/**
 * @brief Initialize manager config with defaults
 * @param config Config to initialize
 */
MD_API void md_manager_config_init(md_manager_config_t *config);

/* ============================================================================
 * Debugger Detection
 * ============================================================================ */

/**
 * @brief Find a debugger by type
 * @param type Debugger type
 * @param path Buffer to store the path
 * @param size Size of buffer
 * @return Error code
 * 
 * Searches common locations for the specified debugger.
 */
MD_API md_error_t md_find_debugger(md_debugger_type_t type, char *path, size_t size);

/**
 * @brief Detect debugger type from path
 * @param path Path to debugger
 * @return Debugger type, or MD_DEBUGGER_NONE if unknown
 */
MD_API md_debugger_type_t md_detect_debugger_type(const char *path);

/**
 * @brief Check if a debugger exists at the given path
 * @param path Path to check
 * @return true if exists and is executable, false otherwise
 */
MD_API bool md_debugger_exists(const char *path);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get state name as string
 * @param state Session state
 * @return Static string representation
 */
MD_API const char* md_session_state_string(md_session_state_t state);

/**
 * @brief Get debugger type name as string
 * @param type Debugger type
 * @return Static string representation
 */
MD_API const char* md_debugger_type_string(md_debugger_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* MAGIC_DEBUGGER_SESSION_MANAGER_H */
