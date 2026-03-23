/**
 * @file types.c
 * @brief Implementation of common types and utility functions
 */

#include "types.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ============================================================================
 * Error Strings
 * ============================================================================ */

static const struct {
    md_error_t code;
    const char *message;
} g_error_strings[] = {
    { MD_SUCCESS,                   "Success" },
    { MD_ERROR_UNKNOWN,             "Unknown error" },
    { MD_ERROR_INVALID_ARG,         "Invalid argument" },
    { MD_ERROR_NULL_POINTER,        "Null pointer" },
    { MD_ERROR_OUT_OF_MEMORY,       "Out of memory" },
    { MD_ERROR_PIPE_FAILED,         "Pipe creation failed" },
    { MD_ERROR_FORK_FAILED,         "Fork failed" },
    { MD_ERROR_EXEC_FAILED,         "Exec failed" },
    { MD_ERROR_SESSION_NOT_FOUND,   "Session not found" },
    { MD_ERROR_SESSION_EXISTS,      "Session already exists" },
    { MD_ERROR_INVALID_STATE,       "Invalid state" },
    { MD_ERROR_IO_ERROR,            "I/O error" },
    { MD_ERROR_TIMEOUT,             "Operation timeout" },
    { MD_ERROR_BUFFER_OVERFLOW,     "Buffer overflow" },
    { MD_ERROR_PROCESS_EXITED,      "Process has exited" },
    { MD_ERROR_PERMISSION,          "Permission denied" },
    { MD_ERROR_NOT_INITIALIZED,     "Subsystem not initialized" },
    { MD_ERROR_ALREADY_INITIALIZED, "Already initialized" },
    { MD_ERROR_CONNECTION_CLOSED,   "Connection closed" },
    { MD_ERROR_INVALID_SEQ,         "Invalid sequence number" },
    { MD_ERROR_REQUEST_FAILED,      "Request failed" },
    { MD_ERROR_NOT_FOUND,           "Resource not found" },
    { MD_ERROR_BUFFER_TOO_SMALL,    "Buffer too small" },
    { MD_ERROR_NOT_SUPPORTED,       "Operation not supported" },
};

/* ============================================================================
 * Error String Lookup
 * ============================================================================ */

const char* md_error_string(md_error_t error) {
    for (size_t i = 0; i < MD_ARRAY_LEN(g_error_strings); i++) {
        if (g_error_strings[i].code == error) {
            return g_error_strings[i].message;
        }
    }
    return "Unknown error code";
}

/* ============================================================================
 * Memory Management
 * ============================================================================ */

void* md_alloc(size_t count, size_t size) {
    if (count == 0 || size == 0) {
        return NULL;
    }
    
    /* Check for overflow */
    size_t total = count * size;
    if (total / count != size) {
        return NULL;  /* Overflow */
    }
    
    return calloc(count, size);
}

void* md_realloc(void *ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

void md_free(void *ptr) {
    free(ptr);
}

char* md_strdup(const char *str) {
    if (str == NULL) {
        return NULL;
    }
    
    size_t len = strlen(str);
    char *dup = (char*)malloc(len + 1);
    if (dup != NULL) {
        memcpy(dup, str, len + 1);
    }
    return dup;
}

char* md_strndup(const char *str, size_t len) {
    if (str == NULL) {
        return NULL;
    }
    
    size_t actual_len = strlen(str);
    if (actual_len < len) {
        len = actual_len;
    }
    
    char *dup = (char*)malloc(len + 1);
    if (dup != NULL) {
        memcpy(dup, str, len);
        dup[len] = '\0';
    }
    return dup;
}
