/**
 * @file signal_handler.h
 * @brief Signal Handling for Magic Debugger
 * 
 * Provides safe signal handling for the debugger, including:
 * - SIGCHLD handling for child process termination
 * - SIGTERM/SIGINT for graceful shutdown
 * - Signal-safe communication mechanisms
 */

#ifndef MAGIC_DEBUGGER_SIGNAL_HANDLER_H
#define MAGIC_DEBUGGER_SIGNAL_HANDLER_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Signal handler context
 */
typedef struct md_signal_context md_signal_context_t;

/**
 * @brief Signal callback function type
 */
typedef void (*md_signal_callback_t)(int signo, void *user_data);

/**
 * @brief Child exit callback function type
 */
typedef void (*md_child_exit_callback_t)(int pid, int status, void *user_data);

/* ============================================================================
 * Signal Handler Initialization
 * ============================================================================ */

/**
 * @brief Initialize the signal handling subsystem
 * @return Error code
 * 
 * Sets up handlers for:
 * - SIGCHLD (child process state changes)
 * - SIGTERM (graceful termination request)
 * - SIGINT (interrupt, usually Ctrl+C)
 * - SIGPIPE (broken pipe, ignored)
 */
MD_API md_error_t md_signal_init(void);

/**
 * @brief Shutdown the signal handling subsystem
 * 
 * Restores default signal handlers and cleans up resources.
 */
MD_API void md_signal_shutdown(void);

/**
 * @brief Check if signal subsystem is initialized
 * @return true if initialized, false otherwise
 */
MD_API bool md_signal_is_initialized(void);

/* ============================================================================
 * Signal-safe Communication
 * ============================================================================ */

/**
 * @brief Create a self-pipe for signal-safe notification
 * @param read_fd Pointer to store read end
 * @param write_fd Pointer to store write end
 * @return Error code
 * 
 * The self-pipe can be used to safely notify the main loop when
 * a signal occurs, allowing signal handlers to be minimal.
 */
MD_API md_error_t md_signal_create_self_pipe(int *read_fd, int *write_fd);

/**
 * @brief Notify via self-pipe (signal-safe)
 * @param write_fd Write end of self-pipe
 * @return 0 on success, -1 on error
 * 
 * This function is async-signal-safe.
 */
MD_API int md_signal_notify(int write_fd);

/**
 * @brief Clear notification from self-pipe
 * @param read_fd Read end of self-pipe
 * @return 0 on success, -1 on error
 */
MD_API int md_signal_clear_notification(int read_fd);

/* ============================================================================
 * SIGCHLD Handling
 * ============================================================================ */

/**
 * @brief Register callback for child process exit
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return Error code
 */
MD_API md_error_t md_signal_set_child_callback(md_child_exit_callback_t callback,
                                                void *user_data);

/**
 * @brief Wait for a specific child process
 * @param pid Process ID to wait for
 * @param status Pointer to store exit status (optional)
 * @param options Wait options (WNOHANG, etc.)
 * @return Process ID of terminated child, 0 if still running, -1 on error
 */
MD_API int md_signal_waitpid(int pid, int *status, int options);

/**
 * @brief Check if a child process has exited
 * @param pid Process ID
 * @param exit_code Pointer to store exit code (optional)
 * @return true if exited, false otherwise
 */
MD_API bool md_signal_child_exited(int pid, int *exit_code);

/**
 * @brief Get the exit code from a status value
 * @param status Status from waitpid
 * @return Exit code
 */
MD_API int md_signal_get_exit_code(int status);

/**
 * @brief Check if the child was terminated by a signal
 * @param status Status from waitpid
 * @return true if terminated by signal
 */
MD_API bool md_signal_was_signaled(int status);

/**
 * @brief Get the terminating signal
 * @param status Status from waitpid
 * @return Signal number, or 0 if not signaled
 */
MD_API int md_signal_get_term_signal(int status);

/**
 * @brief Check if the child was stopped
 * @param status Status from waitpid
 * @return true if stopped
 */
MD_API bool md_signal_was_stopped(int status);

/**
 * @brief Get the stop signal
 * @param status Status from waitpid
 * @return Signal number, or 0 if not stopped
 */
MD_API int md_signal_get_stop_signal(int status);

/* ============================================================================
 * Signal Masking
 * ============================================================================ */

/**
 * @brief Block a signal
 * @param signo Signal number
 * @return Error code
 */
MD_API md_error_t md_signal_block(int signo);

/**
 * @brief Unblock a signal
 * @param signo Signal number
 * @return Error code
 */
MD_API md_error_t md_signal_unblock(int signo);

/**
 * @brief Block multiple signals
 * @param signals Array of signal numbers
 * @param count Number of signals
 * @return Error code
 */
MD_API md_error_t md_signal_block_multiple(const int *signals, int count);

/**
 * @brief Unblock multiple signals
 * @param signals Array of signal numbers
 * @param count Number of signals
 * @return Error code
 */
MD_API md_error_t md_signal_unblock_multiple(const int *signals, int count);

/**
 * @brief Save current signal mask and block signals
 * @param signals Array of signal numbers
 * @param count Number of signals
 * @param old_mask Pointer to store old mask (for restoration)
 * @return Error code
 */
MD_API md_error_t md_signal_save_and_block(const int *signals, int count,
                                            void *old_mask);

/**
 * @brief Restore a previously saved signal mask
 * @param old_mask Pointer to saved mask
 * @return Error code
 */
MD_API md_error_t md_signal_restore_mask(void *old_mask);

/* ============================================================================
 * Graceful Shutdown
 * ============================================================================ */

/**
 * @brief Shutdown callback type
 */
typedef void (*md_shutdown_callback_t)(void *user_data);

/**
 * @brief Register a callback for graceful shutdown
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return Error code
 * 
 * The callback will be invoked when SIGTERM or SIGINT is received.
 */
MD_API md_error_t md_signal_set_shutdown_callback(md_shutdown_callback_t callback,
                                                   void *user_data);

/**
 * @brief Check if shutdown was requested
 * @return true if SIGTERM/SIGINT was received
 */
MD_API bool md_signal_shutdown_requested(void);

/**
 * @brief Clear shutdown request flag
 */
MD_API void md_signal_clear_shutdown_request(void);

/**
 * @brief Request graceful shutdown programmatically
 * @return Error code
 */
MD_API md_error_t md_signal_request_shutdown(void);

/* ============================================================================
 * Signal Information
 * ============================================================================ */

/**
 * @brief Get signal name from number
 * @param signo Signal number
 * @return Signal name (e.g., "SIGTERM"), or NULL if unknown
 */
MD_API const char* md_signal_name(int signo);

/**
 * @brief Get signal number from name
 * @param name Signal name (with or without "SIG" prefix)
 * @return Signal number, or -1 if not found
 */
MD_API int md_signal_from_name(const char *name);

/**
 * @brief Check if a signal is valid
 * @param signo Signal number
 * @return true if valid
 */
MD_API bool md_signal_is_valid(int signo);

/* ============================================================================
 * Process Termination
 * ============================================================================ */

/**
 * @brief Send a signal to a process
 * @param pid Process ID (0 for current process group, -1 for all processes)
 * @param signo Signal number
 * @return Error code
 */
MD_API md_error_t md_signal_kill(int pid, int signo);

/**
 * @brief Terminate a process gracefully (SIGTERM)
 * @param pid Process ID
 * @return Error code
 */
MD_API md_error_t md_signal_terminate(int pid);

/**
 * @brief Forcefully kill a process (SIGKILL)
 * @param pid Process ID
 * @return Error code
 */
MD_API md_error_t md_signal_force_kill(int pid);

/**
 * @brief Interrupt a process (SIGINT)
 * @param pid Process ID
 * @return Error code
 */
MD_API md_error_t md_signal_interrupt(int pid);

/**
 * @brief Continue a stopped process (SIGCONT)
 * @param pid Process ID
 * @return Error code
 */
MD_API md_error_t md_signal_continue(int pid);

/**
 * @brief Stop a process (SIGSTOP)
 * @param pid Process ID
 * @return Error code
 */
MD_API md_error_t md_signal_stop(int pid);

#ifdef __cplusplus
}
#endif

#endif /* MAGIC_DEBUGGER_SIGNAL_HANDLER_H */
