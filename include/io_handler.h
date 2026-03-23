/**
 * @file io_handler.h
 * @brief Non-blocking I/O Handler for Magic Debugger
 * 
 * Provides non-blocking I/O operations for communication with
 * debugger processes using select()/poll().
 */

#ifndef MAGIC_DEBUGGER_IO_HANDLER_H
#define MAGIC_DEBUGGER_IO_HANDLER_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

struct md_io_handler;
struct md_io_buffer;

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Opaque I/O handler handle
 */
typedef struct md_io_handler md_io_handler_t;

/**
 * @brief Opaque I/O buffer handle
 */
typedef struct md_io_buffer md_io_buffer_t;

/* ============================================================================
 * I/O Buffer
 * ============================================================================ */

/**
 * @brief Create a new I/O buffer
 * @param initial_size Initial buffer capacity
 * @return Buffer handle, or NULL on failure
 */
MD_API md_io_buffer_t* md_io_buffer_create(size_t initial_size);

/**
 * @brief Destroy an I/O buffer
 * @param buffer Buffer handle
 */
MD_API void md_io_buffer_destroy(md_io_buffer_t *buffer);

/**
 * @brief Clear the buffer contents
 * @param buffer Buffer handle
 */
MD_API void md_io_buffer_clear(md_io_buffer_t *buffer);

/**
 * @brief Get the current data size
 * @param buffer Buffer handle
 * @return Number of bytes in buffer
 */
MD_API size_t md_io_buffer_size(md_io_buffer_t *buffer);

/**
 * @brief Get the buffer capacity
 * @param buffer Buffer handle
 * @return Buffer capacity in bytes
 */
MD_API size_t md_io_buffer_capacity(md_io_buffer_t *buffer);

/**
 * @brief Get pointer to buffer data (read-only)
 * @param buffer Buffer handle
 * @return Pointer to data, or NULL if empty
 */
MD_API const void* md_io_buffer_data(md_io_buffer_t *buffer);

/**
 * @brief Write data to the buffer
 * @param buffer Buffer handle
 * @param data Data to write
 * @param size Size of data
 * @return Number of bytes written, or negative error code
 */
MD_API ssize_t md_io_buffer_write(md_io_buffer_t *buffer, const void *data, size_t size);

/**
 * @brief Read data from the buffer (removes from front)
 * @param buffer Buffer handle
 * @param data Buffer to read into
 * @param size Maximum bytes to read
 * @return Number of bytes read, or negative error code
 */
MD_API ssize_t md_io_buffer_read(md_io_buffer_t *buffer, void *data, size_t size);

/**
 * @brief Peek at data without removing
 * @param buffer Buffer handle
 * @param data Buffer to read into
 * @param size Maximum bytes to peek
 * @return Number of bytes peeked, or negative error code
 */
MD_API ssize_t md_io_buffer_peek(md_io_buffer_t *buffer, void *data, size_t size);

/**
 * @brief Find a line in the buffer
 * @param buffer Buffer handle
 * @param line_len Pointer to store line length (including newline)
 * @return true if a complete line exists, false otherwise
 */
MD_API bool md_io_buffer_has_line(md_io_buffer_t *buffer, size_t *line_len);

/**
 * @brief Read a line from the buffer
 * @param buffer Buffer handle
 * @param line Buffer to store line
 * @param size Size of buffer
 * @return Number of bytes read, or negative error code
 */
MD_API ssize_t md_io_buffer_read_line(md_io_buffer_t *buffer, char *line, size_t size);

/**
 * @brief Compact the buffer (move data to front)
 * @param buffer Buffer handle
 */
MD_API void md_io_buffer_compact(md_io_buffer_t *buffer);

/* ============================================================================
 * Non-blocking File Descriptor Operations
 * ============================================================================ */

/**
 * @brief Set a file descriptor to non-blocking mode
 * @param fd File descriptor
 * @return Error code
 */
MD_API md_error_t md_set_nonblocking(int fd);

/**
 * @brief Set a file descriptor to blocking mode
 * @param fd File descriptor
 * @return Error code
 */
MD_API md_error_t md_set_blocking(int fd);

/**
 * @brief Check if a file descriptor is non-blocking
 * @param fd File descriptor
 * @return true if non-blocking, false otherwise (including error)
 */
MD_API bool md_is_nonblocking(int fd);

/**
 * @brief Close a file descriptor safely
 * @param fd File descriptor (closed and set to -1)
 */
MD_API void md_close_fd(int *fd);

/**
 * @brief Read from a non-blocking file descriptor
 * @param fd File descriptor
 * @param buffer Buffer to read into
 * @param size Maximum bytes to read
 * @return Number of bytes read, 0 if no data, or negative error code
 */
MD_API ssize_t md_nb_read(int fd, void *buffer, size_t size);

/**
 * @brief Write to a non-blocking file descriptor
 * @param fd File descriptor
 * @param data Data to write
 * @param size Size of data
 * @return Number of bytes written, or negative error code
 */
MD_API ssize_t md_nb_write(int fd, const void *data, size_t size);

/**
 * @brief Write all data (handles partial writes)
 * @param fd File descriptor
 * @param data Data to write
 * @param size Size of data
 * @return Number of bytes written, or negative error code
 */
MD_API ssize_t md_nb_write_all(int fd, const void *data, size_t size);

/**
 * @brief Read until a delimiter is found
 * @param fd File descriptor
 * @param buffer Buffer to read into
 * @param size Size of buffer
 * @param delim Delimiter character
 * @param bytes_read Pointer to store bytes read (optional)
 * @return Error code
 */
MD_API md_error_t md_nb_read_until(int fd, char *buffer, size_t size,
                                    char delim, size_t *bytes_read);

/* ============================================================================
 * I/O Multiplexing
 * ============================================================================ */

/**
 * @brief I/O event structure for polling
 */
typedef struct {
    int fd;                 /**< File descriptor */
    int events;             /**< Events to watch (md_io_event_t flags) */
    int revents;            /**< Events that occurred */
} md_poll_event_t;

/**
 * @brief Poll for I/O events
 * @param events Array of poll events
 * @param count Number of events
 * @param timeout_ms Timeout in milliseconds (-1 for infinite)
 * @return Number of ready descriptors, or negative error code
 */
MD_API int md_poll(md_poll_event_t *events, int count, int timeout_ms);

/**
 * @brief Wait for data to be available for reading
 * @param fd File descriptor
 * @param timeout_ms Timeout in milliseconds (-1 for infinite)
 * @return true if data is available, false on timeout, negative on error
 */
MD_API int md_wait_readable(int fd, int timeout_ms);

/**
 * @brief Wait for file descriptor to be ready for writing
 * @param fd File descriptor
 * @param timeout_ms Timeout in milliseconds (-1 for infinite)
 * @return true if ready, false on timeout, negative on error
 */
MD_API int md_wait_writable(int fd, int timeout_ms);

/* ============================================================================
 * Pipe Operations
 * ============================================================================ */

/**
 * @brief Create a pipe
 * @param read_fd Pointer to store read end
 * @param write_fd Pointer to store write end
 * @param nonblocking If true, set both ends to non-blocking
 * @return Error code
 */
MD_API md_error_t md_pipe_create(int *read_fd, int *write_fd, bool nonblocking);

/**
 * @brief Create a pipe with close-on-exec flag
 * @param read_fd Pointer to store read end
 * @param write_fd Pointer to store write end
 * @param nonblocking If true, set both ends to non-blocking
 * @return Error code
 */
MD_API md_error_t md_pipe_create_cloexec(int *read_fd, int *write_fd, bool nonblocking);

/* ============================================================================
 * I/O Handler (Event-based)
 * ============================================================================ */

/**
 * @brief Callback for I/O events
 */
typedef void (*md_io_event_callback_t)(int fd, int events, void *user_data);

/**
 * @brief Create an I/O handler
 * @param max_fds Maximum number of file descriptors to monitor
 * @return Handler handle, or NULL on failure
 */
MD_API md_io_handler_t* md_io_handler_create(int max_fds);

/**
 * @brief Destroy an I/O handler
 * @param handler Handler handle
 */
MD_API void md_io_handler_destroy(md_io_handler_t *handler);

/**
 * @brief Register a file descriptor for monitoring
 * @param handler Handler handle
 * @param fd File descriptor
 * @param events Events to monitor (md_io_event_t flags)
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return Error code
 */
MD_API md_error_t md_io_handler_register(md_io_handler_t *handler, int fd,
                                          int events, md_io_event_callback_t callback,
                                          void *user_data);

/**
 * @brief Unregister a file descriptor
 * @param handler Handler handle
 * @param fd File descriptor
 * @return Error code
 */
MD_API md_error_t md_io_handler_unregister(md_io_handler_t *handler, int fd);

/**
 * @brief Modify events for a registered file descriptor
 * @param handler Handler handle
 * @param fd File descriptor
 * @param events New events to monitor
 * @return Error code
 */
MD_API md_error_t md_io_handler_modify(md_io_handler_t *handler, int fd, int events);

/**
 * @brief Process events (blocking)
 * @param handler Handler handle
 * @param timeout_ms Timeout in milliseconds (-1 for infinite)
 * @return Number of events processed, or negative error code
 */
MD_API int md_io_handler_process(md_io_handler_t *handler, int timeout_ms);

/**
 * @brief Check if a file descriptor is registered
 * @param handler Handler handle
 * @param fd File descriptor
 * @return true if registered, false otherwise
 */
MD_API bool md_io_handler_is_registered(md_io_handler_t *handler, int fd);

/**
 * @brief Get the number of registered file descriptors
 * @param handler Handler handle
 * @return Number of registered file descriptors
 */
MD_API int md_io_handler_count(md_io_handler_t *handler);

#ifdef __cplusplus
}
#endif

#endif /* MAGIC_DEBUGGER_IO_HANDLER_H */
