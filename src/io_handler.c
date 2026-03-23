/**
 * @file io_handler.c
 * @brief Implementation of non-blocking I/O operations
 */

#include "io_handler.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

/* ============================================================================
 * I/O Buffer Implementation
 * ============================================================================ */

struct md_io_buffer {
    char *data;             /**< Buffer data */
    size_t size;            /**< Current data size */
    size_t capacity;        /**< Buffer capacity */
    size_t head;            /**< Read position */
    size_t tail;            /**< Write position */
};

md_io_buffer_t* md_io_buffer_create(size_t initial_size) {
    md_io_buffer_t *buffer = (md_io_buffer_t*)calloc(1, sizeof(md_io_buffer_t));
    if (buffer == NULL) {
        return NULL;
    }
    
    if (initial_size == 0) {
        initial_size = MD_IO_BUFFER_SIZE;
    }
    
    buffer->data = (char*)malloc(initial_size);
    if (buffer->data == NULL) {
        free(buffer);
        return NULL;
    }
    
    buffer->capacity = initial_size;
    buffer->size = 0;
    buffer->head = 0;
    buffer->tail = 0;
    
    return buffer;
}

void md_io_buffer_destroy(md_io_buffer_t *buffer) {
    if (buffer == NULL) {
        return;
    }
    
    free(buffer->data);
    free(buffer);
}

void md_io_buffer_clear(md_io_buffer_t *buffer) {
    if (buffer == NULL) {
        return;
    }
    
    buffer->size = 0;
    buffer->head = 0;
    buffer->tail = 0;
}

size_t md_io_buffer_size(md_io_buffer_t *buffer) {
    if (buffer == NULL) {
        return 0;
    }
    return buffer->size;
}

size_t md_io_buffer_capacity(md_io_buffer_t *buffer) {
    if (buffer == NULL) {
        return 0;
    }
    return buffer->capacity;
}

const void* md_io_buffer_data(md_io_buffer_t *buffer) {
    if (buffer == NULL || buffer->size == 0) {
        return NULL;
    }
    return buffer->data + buffer->head;
}

ssize_t md_io_buffer_write(md_io_buffer_t *buffer, const void *data, size_t size) {
    if (buffer == NULL || data == NULL || size == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    /* Check if we need to expand */
    if (buffer->size + size > buffer->capacity) {
        /* Try to compact first */
        if (buffer->head > 0 && buffer->capacity - buffer->size >= size) {
            md_io_buffer_compact(buffer);
        } else {
            /* Need to expand */
            size_t new_capacity = buffer->capacity * 2;
            if (new_capacity < buffer->size + size) {
                new_capacity = buffer->size + size;
            }
            
            char *new_data = (char*)realloc(buffer->data, new_capacity);
            if (new_data == NULL) {
                return MD_ERROR_OUT_OF_MEMORY;
            }
            
            buffer->data = new_data;
            buffer->capacity = new_capacity;
        }
    }
    
    /* Copy data */
    if (buffer->tail + size <= buffer->capacity) {
        /* Contiguous write */
        memcpy(buffer->data + buffer->tail, data, size);
        buffer->tail += size;
    } else {
        /* Wrapped write */
        size_t first_part = buffer->capacity - buffer->tail;
        memcpy(buffer->data + buffer->tail, data, first_part);
        memcpy(buffer->data, (char*)data + first_part, size - first_part);
        buffer->tail = size - first_part;
    }
    
    buffer->size += size;
    return (ssize_t)size;
}

ssize_t md_io_buffer_read(md_io_buffer_t *buffer, void *data, size_t size) {
    if (buffer == NULL || data == NULL || size == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    if (buffer->size == 0) {
        return 0;
    }
    
    size_t to_read = (size > buffer->size) ? buffer->size : size;
    
    if (buffer->head + to_read <= buffer->capacity) {
        /* Contiguous read */
        memcpy(data, buffer->data + buffer->head, to_read);
        buffer->head += to_read;
    } else {
        /* Wrapped read */
        size_t first_part = buffer->capacity - buffer->head;
        memcpy(data, buffer->data + buffer->head, first_part);
        memcpy((char*)data + first_part, buffer->data, to_read - first_part);
        buffer->head = to_read - first_part;
    }
    
    buffer->size -= to_read;
    
    /* Auto-compact if nearly empty */
    if (buffer->size == 0) {
        buffer->head = 0;
        buffer->tail = 0;
    }
    
    return (ssize_t)to_read;
}

ssize_t md_io_buffer_peek(md_io_buffer_t *buffer, void *data, size_t size) {
    if (buffer == NULL || data == NULL || size == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    if (buffer->size == 0) {
        return 0;
    }
    
    size_t to_peek = (size > buffer->size) ? buffer->size : size;
    
    if (buffer->head + to_peek <= buffer->capacity) {
        memcpy(data, buffer->data + buffer->head, to_peek);
    } else {
        size_t first_part = buffer->capacity - buffer->head;
        memcpy(data, buffer->data + buffer->head, first_part);
        memcpy((char*)data + first_part, buffer->data, to_peek - first_part);
    }
    
    return (ssize_t)to_peek;
}

bool md_io_buffer_has_line(md_io_buffer_t *buffer, size_t *line_len) {
    if (buffer == NULL || buffer->size == 0) {
        if (line_len != NULL) {
            *line_len = 0;
        }
        return false;
    }
    
    /* Search for newline */
    for (size_t i = 0; i < buffer->size; i++) {
        size_t idx = (buffer->head + i) % buffer->capacity;
        if (buffer->data[idx] == '\n') {
            if (line_len != NULL) {
                *line_len = i + 1;
            }
            return true;
        }
    }
    
    if (line_len != NULL) {
        *line_len = 0;
    }
    return false;
}

ssize_t md_io_buffer_read_line(md_io_buffer_t *buffer, char *line, size_t size) {
    size_t line_len;
    
    if (!md_io_buffer_has_line(buffer, &line_len)) {
        return 0;
    }
    
    if (line_len >= size) {
        return MD_ERROR_BUFFER_OVERFLOW;
    }
    
    ssize_t read = md_io_buffer_read(buffer, line, line_len);
    if (read > 0) {
        line[read] = '\0';
    }
    
    return read;
}

void md_io_buffer_compact(md_io_buffer_t *buffer) {
    if (buffer == NULL || buffer->size == 0 || buffer->head == 0) {
        return;
    }
    
    /* Move data to the front */
    if (buffer->head + buffer->size <= buffer->capacity) {
        memmove(buffer->data, buffer->data + buffer->head, buffer->size);
    } else {
        /* Handle wrapped data */
        size_t first_part = buffer->capacity - buffer->head;
        char *temp = (char*)malloc(first_part);
        if (temp == NULL) {
            return;
        }
        
        memcpy(temp, buffer->data + buffer->head, first_part);
        memmove(buffer->data + first_part, buffer->data, buffer->size - first_part);
        memcpy(buffer->data, temp, first_part);
        free(temp);
    }
    
    buffer->head = 0;
    buffer->tail = buffer->size;
}

/* ============================================================================
 * Non-blocking FD Operations
 * ============================================================================ */

md_error_t md_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return MD_ERROR_IO_ERROR;
    }
    
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return MD_ERROR_IO_ERROR;
    }
    
    return MD_SUCCESS;
}

md_error_t md_set_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return MD_ERROR_IO_ERROR;
    }
    
    if (fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) < 0) {
        return MD_ERROR_IO_ERROR;
    }
    
    return MD_SUCCESS;
}

bool md_is_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return (flags & O_NONBLOCK) != 0;
}

void md_close_fd(int *fd) {
    if (fd != NULL && *fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

ssize_t md_nb_read(int fd, void *buffer, size_t size) {
    if (fd < 0 || buffer == NULL || size == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    ssize_t bytes = read(fd, buffer, size);
    
    if (bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  /* No data available */
        }
        return MD_ERROR_IO_ERROR;
    }
    
    return bytes;
}

ssize_t md_nb_write(int fd, const void *data, size_t size) {
    if (fd < 0 || data == NULL || size == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    ssize_t bytes = write(fd, data, size);
    
    if (bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  /* Would block */
        }
        return MD_ERROR_IO_ERROR;
    }
    
    return bytes;
}

ssize_t md_nb_write_all(int fd, const void *data, size_t size) {
    if (fd < 0 || data == NULL || size == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    size_t total_written = 0;
    const char *ptr = (const char *)data;
    
    while (total_written < size) {
        ssize_t written = write(fd, ptr + total_written, size - total_written);
        
        if (written < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* Wait for fd to become writable */
                int result = md_wait_writable(fd, MD_DEFAULT_TIMEOUT_MS);
                if (result <= 0) {
                    if (result == 0) {
                        return MD_ERROR_TIMEOUT;
                    }
                    return MD_ERROR_IO_ERROR;
                }
                continue;
            }
            return MD_ERROR_IO_ERROR;
        }
        
        total_written += written;
    }
    
    return (ssize_t)total_written;
}

md_error_t md_nb_read_until(int fd, char *buffer, size_t size,
                            char delim, size_t *bytes_read) {
    if (fd < 0 || buffer == NULL || size == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    size_t total_read = 0;
    
    while (total_read < size - 1) {
        char c;
        ssize_t n = read(fd, &c, 1);
        
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (bytes_read != NULL) {
                    *bytes_read = total_read;
                }
                buffer[total_read] = '\0';
                return MD_SUCCESS;
            }
            return MD_ERROR_IO_ERROR;
        }
        
        if (n == 0) {
            /* EOF */
            break;
        }
        
        buffer[total_read++] = c;
        
        if (c == delim) {
            break;
        }
    }
    
    buffer[total_read] = '\0';
    
    if (bytes_read != NULL) {
        *bytes_read = total_read;
    }
    
    return MD_SUCCESS;
}

/* ============================================================================
 * I/O Multiplexing
 * ============================================================================ */

int md_poll(md_poll_event_t *events, int count, int timeout_ms) {
    if (events == NULL || count <= 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    struct pollfd *pfds = (struct pollfd*)calloc(count, sizeof(struct pollfd));
    if (pfds == NULL) {
        return MD_ERROR_OUT_OF_MEMORY;
    }
    
    /* Convert to pollfd */
    for (int i = 0; i < count; i++) {
        pfds[i].fd = events[i].fd;
        pfds[i].events = 0;
        
        if (events[i].events & MD_IO_EVENT_READ) {
            pfds[i].events |= POLLIN;
        }
        if (events[i].events & MD_IO_EVENT_WRITE) {
            pfds[i].events |= POLLOUT;
        }
        if (events[i].events & MD_IO_EVENT_ERROR) {
            pfds[i].events |= POLLERR;
        }
        if (events[i].events & MD_IO_EVENT_HANGUP) {
            pfds[i].events |= POLLHUP;
        }
        
        pfds[i].revents = 0;
    }
    
    /* Call poll */
    int result = poll(pfds, count, timeout_ms);
    
    /* Convert back */
    if (result > 0) {
        for (int i = 0; i < count; i++) {
            events[i].revents = 0;
            
            if (pfds[i].revents & POLLIN) {
                events[i].revents |= MD_IO_EVENT_READ;
            }
            if (pfds[i].revents & POLLOUT) {
                events[i].revents |= MD_IO_EVENT_WRITE;
            }
            if (pfds[i].revents & POLLERR) {
                events[i].revents |= MD_IO_EVENT_ERROR;
            }
            if (pfds[i].revents & POLLHUP) {
                events[i].revents |= MD_IO_EVENT_HANGUP;
            }
        }
    }
    
    free(pfds);
    return result;
}

int md_wait_readable(int fd, int timeout_ms) {
    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN,
        .revents = 0,
    };
    
    int result = poll(&pfd, 1, timeout_ms);
    
    if (result < 0) {
        return MD_ERROR_IO_ERROR;
    }
    
    if (result == 0) {
        return 0;  /* Timeout */
    }
    
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        return MD_ERROR_IO_ERROR;
    }
    
    return 1;  /* Ready */
}

int md_wait_writable(int fd, int timeout_ms) {
    struct pollfd pfd = {
        .fd = fd,
        .events = POLLOUT,
        .revents = 0,
    };
    
    int result = poll(&pfd, 1, timeout_ms);
    
    if (result < 0) {
        return MD_ERROR_IO_ERROR;
    }
    
    if (result == 0) {
        return 0;  /* Timeout */
    }
    
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        return MD_ERROR_IO_ERROR;
    }
    
    return 1;  /* Ready */
}

/* ============================================================================
 * Pipe Operations
 * ============================================================================ */

md_error_t md_pipe_create(int *read_fd, int *write_fd, bool nonblocking) {
    if (read_fd == NULL || write_fd == NULL) {
        return MD_ERROR_NULL_POINTER;
    }
    
    int fds[2];
    if (pipe(fds) < 0) {
        return MD_ERROR_PIPE_FAILED;
    }
    
    *read_fd = fds[0];
    *write_fd = fds[1];
    
    if (nonblocking) {
        md_set_nonblocking(*read_fd);
        md_set_nonblocking(*write_fd);
    }
    
    return MD_SUCCESS;
}

md_error_t md_pipe_create_cloexec(int *read_fd, int *write_fd, bool nonblocking) {
    if (read_fd == NULL || write_fd == NULL) {
        return MD_ERROR_NULL_POINTER;
    }
    
#if defined(O_CLOEXEC) && defined(pipe2)
    int fds[2];
    if (pipe2(fds, O_CLOEXEC) < 0) {
        return MD_ERROR_PIPE_FAILED;
    }
    
    *read_fd = fds[0];
    *write_fd = fds[1];
#else
    md_error_t err = md_pipe_create(read_fd, write_fd, false);
    if (err != MD_SUCCESS) {
        return err;
    }
    
    /* Set close-on-exec manually */
    fcntl(*read_fd, F_SETFD, FD_CLOEXEC);
    fcntl(*write_fd, F_SETFD, FD_CLOEXEC);
#endif
    
    if (nonblocking) {
        md_set_nonblocking(*read_fd);
        md_set_nonblocking(*write_fd);
    }
    
    return MD_SUCCESS;
}

/* ============================================================================
 * I/O Handler (Event-based)
 * ============================================================================ */

struct md_io_handler {
    int max_fds;
    int count;
    
    struct {
        int fd;
        int events;
        md_io_event_callback_t callback;
        void *user_data;
    } *registrations;
};

md_io_handler_t* md_io_handler_create(int max_fds) {
    if (max_fds <= 0) {
        max_fds = 32;
    }
    
    md_io_handler_t *handler = (md_io_handler_t*)calloc(1, sizeof(md_io_handler_t));
    if (handler == NULL) {
        return NULL;
    }
    
    handler->registrations = calloc(max_fds, sizeof(*handler->registrations));
    if (handler->registrations == NULL) {
        free(handler);
        return NULL;
    }
    
    handler->max_fds = max_fds;
    handler->count = 0;
    
    return handler;
}

void md_io_handler_destroy(md_io_handler_t *handler) {
    if (handler == NULL) {
        return;
    }
    
    free(handler->registrations);
    free(handler);
}

md_error_t md_io_handler_register(md_io_handler_t *handler, int fd,
                                   int events, md_io_event_callback_t callback,
                                   void *user_data) {
    if (handler == NULL || fd < 0 || callback == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    /* Check if already registered */
    for (int i = 0; i < handler->count; i++) {
        if (handler->registrations[i].fd == fd) {
            return MD_ERROR_SESSION_EXISTS;
        }
    }
    
    /* Check capacity */
    if (handler->count >= handler->max_fds) {
        return MD_ERROR_BUFFER_OVERFLOW;
    }
    
    /* Add registration */
    handler->registrations[handler->count].fd = fd;
    handler->registrations[handler->count].events = events;
    handler->registrations[handler->count].callback = callback;
    handler->registrations[handler->count].user_data = user_data;
    handler->count++;
    
    return MD_SUCCESS;
}

md_error_t md_io_handler_unregister(md_io_handler_t *handler, int fd) {
    if (handler == NULL || fd < 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    for (int i = 0; i < handler->count; i++) {
        if (handler->registrations[i].fd == fd) {
            /* Move last item to this position */
            handler->registrations[i] = handler->registrations[handler->count - 1];
            handler->count--;
            return MD_SUCCESS;
        }
    }
    
    return MD_ERROR_SESSION_NOT_FOUND;
}

md_error_t md_io_handler_modify(md_io_handler_t *handler, int fd, int events) {
    if (handler == NULL || fd < 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    for (int i = 0; i < handler->count; i++) {
        if (handler->registrations[i].fd == fd) {
            handler->registrations[i].events = events;
            return MD_SUCCESS;
        }
    }
    
    return MD_ERROR_SESSION_NOT_FOUND;
}

int md_io_handler_process(md_io_handler_t *handler, int timeout_ms) {
    if (handler == NULL || handler->count == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    /* Build pollfd array */
    struct pollfd *pfds = (struct pollfd*)calloc(handler->count, sizeof(struct pollfd));
    if (pfds == NULL) {
        return MD_ERROR_OUT_OF_MEMORY;
    }
    
    for (int i = 0; i < handler->count; i++) {
        pfds[i].fd = handler->registrations[i].fd;
        pfds[i].events = 0;
        
        if (handler->registrations[i].events & MD_IO_EVENT_READ) {
            pfds[i].events |= POLLIN;
        }
        if (handler->registrations[i].events & MD_IO_EVENT_WRITE) {
            pfds[i].events |= POLLOUT;
        }
    }
    
    /* Poll */
    int result = poll(pfds, handler->count, timeout_ms);
    
    if (result <= 0) {
        free(pfds);
        return result;
    }
    
    /* Dispatch events */
    int dispatched = 0;
    for (int i = 0; i < handler->count && dispatched < result; i++) {
        int revents = 0;
        
        if (pfds[i].revents & POLLIN) {
            revents |= MD_IO_EVENT_READ;
        }
        if (pfds[i].revents & POLLOUT) {
            revents |= MD_IO_EVENT_WRITE;
        }
        if (pfds[i].revents & POLLERR) {
            revents |= MD_IO_EVENT_ERROR;
        }
        if (pfds[i].revents & POLLHUP) {
            revents |= MD_IO_EVENT_HANGUP;
        }
        
        if (revents != 0) {
            handler->registrations[i].callback(pfds[i].fd, revents,
                                                handler->registrations[i].user_data);
            dispatched++;
        }
    }
    
    free(pfds);
    return dispatched;
}

bool md_io_handler_is_registered(md_io_handler_t *handler, int fd) {
    if (handler == NULL) {
        return false;
    }
    
    for (int i = 0; i < handler->count; i++) {
        if (handler->registrations[i].fd == fd) {
            return true;
        }
    }
    
    return false;
}

int md_io_handler_count(md_io_handler_t *handler) {
    if (handler == NULL) {
        return 0;
    }
    return handler->count;
}
