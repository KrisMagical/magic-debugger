/**
 * @file types.h
 * @brief Common type definitions for Magic Debugger
 * 
 * This file contains common type definitions, macros, and constants
 * used throughout the Magic Debugger project.
 */

#ifndef MAGIC_DEBUGGER_TYPES_H
#define MAGIC_DEBUGGER_TYPES_H

/* POSIX and system headers */
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>  /* For ssize_t, pid_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Version Information
 * ============================================================================ */

#define MAGIC_DEBUGGER_VERSION_MAJOR 0
#define MAGIC_DEBUGGER_VERSION_MINOR 1
#define MAGIC_DEBUGGER_VERSION_PATCH 0
#define MAGIC_DEBUGGER_VERSION "0.1.0"

/* ============================================================================
 * Platform Detection
 * ============================================================================ */

#if defined(__linux__)
    #define MD_PLATFORM_LINUX    1
    #define MD_PLATFORM_MACOS    0
    #define MD_PLATFORM_WINDOWS  0
#elif defined(__APPLE__)
    #define MD_PLATFORM_LINUX    0
    #define MD_PLATFORM_MACOS    1
    #define MD_PLATFORM_WINDOWS  0
#elif defined(_WIN32) || defined(_WIN64)
    #define MD_PLATFORM_LINUX    0
    #define MD_PLATFORM_MACOS    0
    #define MD_PLATFORM_WINDOWS  1
#else
    #error "Unsupported platform"
#endif

/* ============================================================================
 * Export/Import Macros
 * ============================================================================ */

#if MD_PLATFORM_WINDOWS
    #ifdef MD_BUILD_SHARED
        #define MD_API __declspec(dllexport)
    #else
        #define MD_API __declspec(dllimport)
    #endif
#else
    #define MD_API __attribute__((visibility("default")))
#endif

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

/** Maximum length of a file path */
#define MD_MAX_PATH_LEN         4096

/** Maximum length of a command line argument */
#define MD_MAX_ARG_LEN          1024

/** Maximum number of command line arguments */
#define MD_MAX_ARGS             64

/** Maximum buffer size for I/O operations */
#define MD_IO_BUFFER_SIZE       65536

/** Maximum number of concurrent debug sessions */
#define MD_MAX_SESSIONS         16

/** Default timeout for I/O operations (milliseconds) */
#define MD_DEFAULT_TIMEOUT_MS   5000

/** Maximum length for error messages */
#define MD_MAX_ERROR_LEN        512

/** Maximum length for debugger names */
#define MD_MAX_DEBUGGER_NAME    32

/* ============================================================================
 * Error Codes
 * ============================================================================ */

/**
 * @brief Error codes used throughout Magic Debugger
 */
typedef enum {
    MD_SUCCESS                  = 0,    /**< Operation successful */
    MD_ERROR_UNKNOWN            = -1,   /**< Unknown error */
    MD_ERROR_INVALID_ARG        = -2,   /**< Invalid argument */
    MD_ERROR_NULL_POINTER       = -3,   /**< Null pointer */
    MD_ERROR_OUT_OF_MEMORY      = -4,   /**< Memory allocation failed */
    MD_ERROR_PIPE_FAILED        = -5,   /**< Pipe creation failed */
    MD_ERROR_FORK_FAILED        = -6,   /**< Fork failed */
    MD_ERROR_EXEC_FAILED        = -7,   /**< Exec failed */
    MD_ERROR_SESSION_NOT_FOUND  = -8,   /**< Session not found */
    MD_ERROR_SESSION_EXISTS     = -9,   /**< Session already exists */
    MD_ERROR_INVALID_STATE      = -10,  /**< Invalid session state */
    MD_ERROR_IO_ERROR           = -11,  /**< I/O error */
    MD_ERROR_TIMEOUT            = -12,  /**< Operation timeout */
    MD_ERROR_BUFFER_OVERFLOW    = -13,  /**< Buffer overflow */
    MD_ERROR_PROCESS_EXITED     = -14,  /**< Process has exited */
    MD_ERROR_PERMISSION         = -15,  /**< Permission denied */
    MD_ERROR_NOT_INITIALIZED    = -16,  /**< Subsystem not initialized */
    MD_ERROR_ALREADY_INITIALIZED = -17, /**< Already initialized */
    MD_ERROR_CONNECTION_CLOSED  = -18,  /**< Connection closed */
    MD_ERROR_INVALID_SEQ        = -19,  /**< Invalid sequence number */
    MD_ERROR_REQUEST_FAILED     = -20,  /**< Request failed */
    MD_ERROR_NOT_FOUND          = -21,  /**< Resource not found */
    MD_ERROR_BUFFER_TOO_SMALL   = -22,  /**< Buffer too small */
    MD_ERROR_NOT_SUPPORTED      = -23,  /**< Operation not supported */
} md_error_t;

/* ============================================================================
 * Session States
 * ============================================================================ */

/**
 * @brief Debug session states
 */
typedef enum {
    MD_SESSION_STATE_NONE       = 0,    /**< No state / uninitialized */
    MD_SESSION_STATE_STARTING   = 1,    /**< Session is starting */
    MD_SESSION_STATE_RUNNING    = 2,    /**< Debugger process is running */
    MD_SESSION_STATE_STOPPED    = 3,    /**< Debugger is stopped (at breakpoint) */
    MD_SESSION_STATE_EXITING    = 4,    /**< Session is exiting */
    MD_SESSION_STATE_TERMINATED = 5,    /**< Session has terminated */
} md_session_state_t;

/* ============================================================================
 * Debugger Types
 * ============================================================================ */

/**
 * @brief Supported debugger types
 */
typedef enum {
    MD_DEBUGGER_NONE    = 0,    /**< No debugger */
    MD_DEBUGGER_LLDB    = 1,    /**< LLDB debugger */
    MD_DEBUGGER_GDB     = 2,    /**< GDB debugger */
    MD_DEBUGGER_SHELL   = 3,    /**< Shell debugger (bashdb) */
    MD_DEBUGGER_CUSTOM  = 99,   /**< Custom debugger */
} md_debugger_type_t;

/* ============================================================================
 * I/O Event Types
 * ============================================================================ */

/**
 * @brief I/O event types for callbacks
 */
typedef enum {
    MD_IO_EVENT_READ    = 0x01, /**< Data available for reading */
    MD_IO_EVENT_WRITE   = 0x02, /**< Ready for writing */
    MD_IO_EVENT_ERROR   = 0x04, /**< Error occurred */
    MD_IO_EVENT_HANGUP  = 0x08, /**< Hangup (EOF) */
} md_io_event_t;

/* ============================================================================
 * Function Pointer Types
 * ============================================================================ */

/**
 * @brief Callback for I/O events
 * @param fd File descriptor that triggered the event
 * @param events The events that occurred
 * @param data User-provided data pointer
 */
typedef void (*md_io_callback_t)(int fd, int events, void *data);

/**
 * @brief Callback for process exit events
 * @param pid Process ID that exited
 * @param exit_code Exit code of the process
 * @param data User-provided data pointer
 */
typedef void (*md_exit_callback_t)(int pid, int exit_code, void *data);

/* ============================================================================
 * Utility Macros
 * ============================================================================ */

/** Get the minimum of two values */
#define MD_MIN(a, b) ((a) < (b) ? (a) : (b))

/** Get the maximum of two values */
#define MD_MAX(a, b) ((a) > (b) ? (a) : (b))

/** Get the length of an array */
#define MD_ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

/** Check if a pointer is valid */
#define MD_VALIDATE_PTR(ptr) \
    do { \
        if ((ptr) == NULL) { \
            return MD_ERROR_NULL_POINTER; \
        } \
    } while (0)

/** Safe free macro that nullifies the pointer */
#define MD_SAFE_FREE(ptr) \
    do { \
        if ((ptr) != NULL) { \
            free((ptr)); \
            (ptr) = NULL; \
        } \
    } while (0)

/** Mark a variable as unused */
#define MD_UNUSED(x) ((void)(x))

/** Stringify a macro value */
#define MD_STRINGIFY_(x) #x
#define MD_STRINGIFY(x) MD_STRINGIFY_(x)

/* ============================================================================
 * Error Handling
 * ============================================================================ */

/**
 * @brief Get human-readable error message
 * @param error Error code
 * @return Static string describing the error
 */
MD_API const char* md_error_string(md_error_t error);

/* ============================================================================
 * Memory Management Helpers
 * ============================================================================ */

/**
 * @brief Allocate zeroed memory
 * @param count Number of elements
 * @param size Size of each element
 * @return Pointer to allocated memory, or NULL on failure
 */
MD_API void* md_alloc(size_t count, size_t size);

/**
 * @brief Reallocate memory
 * @param ptr Existing pointer (may be NULL)
 * @param size New size in bytes
 * @return Pointer to reallocated memory, or NULL on failure
 */
MD_API void* md_realloc(void *ptr, size_t size);

/**
 * @brief Free memory allocated by md_alloc/md_realloc
 * @param ptr Pointer to free
 */
MD_API void md_free(void *ptr);

/**
 * @brief Duplicate a string
 * @param str String to duplicate
 * @return Duplicated string, or NULL on failure
 */
MD_API char* md_strdup(const char *str);

/**
 * @brief Duplicate a string with length limit
 * @param str String to duplicate
 * @param len Maximum length to copy
 * @return Duplicated string, or NULL on failure
 */
MD_API char* md_strndup(const char *str, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* MAGIC_DEBUGGER_TYPES_H */
