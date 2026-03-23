/**
 * @file logger.h
 * @brief Logging System for Magic Debugger
 * 
 * Provides a flexible logging system with multiple log levels,
 * output destinations, and formatting options.
 */

#ifndef MAGIC_DEBUGGER_LOGGER_H
#define MAGIC_DEBUGGER_LOGGER_H

#include "types.h"
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Log Levels
 * ============================================================================ */

/**
 * @brief Log severity levels
 */
typedef enum {
    MD_LOG_TRACE   = 0,   /**< Very detailed debugging information */
    MD_LOG_DEBUG   = 1,   /**< Debugging information */
    MD_LOG_INFO    = 2,   /**< Normal operational information */
    MD_LOG_WARNING = 3,   /**< Warning conditions */
    MD_LOG_ERROR   = 4,   /**< Error conditions */
    MD_LOG_FATAL   = 5,   /**< Fatal errors (will exit) */
    MD_LOG_NONE    = 6,   /**< Disable all logging */
} md_log_level_t;

/* ============================================================================
 * Log Destinations
 * ============================================================================ */

/**
 * @brief Log output destinations
 */
typedef enum {
    MD_LOG_DEST_NONE     = 0,       /**< No output */
    MD_LOG_DEST_STDOUT   = (1 << 0), /**< Output to stdout */
    MD_LOG_DEST_STDERR   = (1 << 1), /**< Output to stderr */
    MD_LOG_DEST_FILE     = (1 << 2), /**< Output to file */
    MD_LOG_DEST_SYSLOG   = (1 << 3), /**< Output to syslog */
    MD_LOG_DEST_CALLBACK = (1 << 4), /**< Output to callback */
    MD_LOG_DEST_DEFAULT  = MD_LOG_DEST_STDERR,
} md_log_dest_t;

/* ============================================================================
 * Log Options
 * ============================================================================ */

/**
 * @brief Log formatting options
 */
typedef enum {
    MD_LOG_OPT_NONE          = 0,        /**< No options */
    MD_LOG_OPT_TIMESTAMP     = (1 << 0), /**< Include timestamp */
    MD_LOG_OPT_LEVEL         = (1 << 1), /**< Include log level */
    MD_LOG_OPT_FILE_LINE     = (1 << 2), /**< Include file and line number */
    MD_LOG_OPT_FUNCTION      = (1 << 3), /**< Include function name */
    MD_LOG_OPT_THREAD_ID     = (1 << 4), /**< Include thread ID */
    MD_LOG_OPT_COLOR         = (1 << 5), /**< Use ANSI colors (if terminal) */
    MD_LOG_OPT_PID           = (1 << 6), /**< Include process ID */
    MD_LOG_OPT_DEFAULT       = MD_LOG_OPT_TIMESTAMP | MD_LOG_OPT_LEVEL | 
                               MD_LOG_OPT_FILE_LINE,
} md_log_opt_t;

/* ============================================================================
 * Log Callback
 * ============================================================================ */

/**
 * @brief Log callback function type
 */
typedef void (*md_log_callback_t)(md_log_level_t level, const char *file,
                                   int line, const char *func,
                                   const char *message, void *user_data);

/* ============================================================================
 * Logger Configuration
 * ============================================================================ */

/**
 * @brief Logger configuration
 */
typedef struct {
    md_log_level_t level;           /**< Minimum level to log */
    md_log_dest_t destinations;     /**< Output destinations */
    md_log_opt_t options;           /**< Formatting options */
    char log_file[MD_MAX_PATH_LEN]; /**< Log file path (if MD_LOG_DEST_FILE) */
    md_log_callback_t callback;     /**< Callback function */
    void *callback_data;            /**< User data for callback */
    bool flush_on_write;            /**< Flush after each write */
    bool quiet_mode;                /**< Suppress output (but call callbacks) */
} md_logger_config_t;

/* ============================================================================
 * Logger Initialization
 * ============================================================================ */

/**
 * @brief Initialize the logging system
 * @param config Configuration (NULL for defaults)
 * @return Error code
 */
MD_API md_error_t md_logger_init(const md_logger_config_t *config);

/**
 * @brief Shutdown the logging system
 */
MD_API void md_logger_shutdown(void);

/**
 * @brief Check if logger is initialized
 * @return true if initialized
 */
MD_API bool md_logger_is_initialized(void);

/**
 * @brief Reconfigure the logger
 * @param config New configuration
 * @return Error code
 */
MD_API md_error_t md_logger_reconfigure(const md_logger_config_t *config);

/**
 * @brief Get current logger configuration
 * @param config Pointer to store configuration
 * @return Error code
 */
MD_API md_error_t md_logger_get_config(md_logger_config_t *config);

/**
 * @brief Initialize a config with defaults
 * @param config Config to initialize
 */
MD_API void md_logger_config_init(md_logger_config_t *config);

/* ============================================================================
 * Log Level Management
 * ============================================================================ */

/**
 * @brief Set the minimum log level
 * @param level Minimum level to log
 * @return Error code
 */
MD_API md_error_t md_logger_set_level(md_log_level_t level);

/**
 * @brief Get the current minimum log level
 * @return Current log level
 */
MD_API md_log_level_t md_logger_get_level(void);

/**
 * @brief Check if a level would be logged
 * @param level Level to check
 * @return true if level >= minimum level
 */
MD_API bool md_logger_would_log(md_log_level_t level);

/**
 * @brief Get log level name
 * @param level Log level
 * @return Level name string
 */
MD_API const char* md_log_level_string(md_log_level_t level);

/**
 * @brief Parse log level from string
 * @param str Level name string
 * @return Log level, or MD_LOG_NONE if invalid
 */
MD_API md_log_level_t md_log_level_from_string(const char *str);

/* ============================================================================
 * Core Logging Functions
 * ============================================================================ */

/**
 * @brief Log a message
 * @param level Log level
 * @param file Source file name
 * @param line Source line number
 * @param func Function name
 * @param format Printf-style format string
 * @param ... Format arguments
 */
MD_API void md_log_write(md_log_level_t level, const char *file, int line,
                          const char *func, const char *format, ...);

/**
 * @brief Log a message (va_list version)
 * @param level Log level
 * @param file Source file name
 * @param line Source line number
 * @param func Function name
 * @param format Printf-style format string
 * @param args Format arguments
 */
MD_API void md_log_write_v(md_log_level_t level, const char *file, int line,
                            const char *func, const char *format, va_list args);

/**
 * @brief Log a hex dump
 * @param level Log level
 * @param file Source file name
 * @param line Source line number
 * @param func Function name
 * @param title Title for the dump
 * @param data Data to dump
 * @param size Size of data
 */
MD_API void md_log_hexdump(md_log_level_t level, const char *file, int line,
                            const char *func, const char *title,
                            const void *data, size_t size);

/* ============================================================================
 * Convenience Macros
 * ============================================================================ */

/**
 * @brief Log a trace message
 */
#define MD_TRACE(...) \
    md_log_write(MD_LOG_TRACE, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Log a debug message
 */
#define MD_DEBUG(...) \
    md_log_write(MD_LOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Log an info message
 */
#define MD_INFO(...) \
    md_log_write(MD_LOG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Log a warning message
 */
#define MD_WARNING(...) \
    md_log_write(MD_LOG_WARNING, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Log an error message
 */
#define MD_ERROR(...) \
    md_log_write(MD_LOG_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)

/**
 * @brief Log a fatal message and exit
 */
#define MD_FATAL(...) \
    do { \
        md_log_write(MD_LOG_FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__); \
        exit(1); \
    } while (0)

/**
 * @brief Log an error with error code
 */
#define MD_LOG_ERROR_CODE(code, ...) \
    do { \
        MD_ERROR("[Error %d: %s] " __VA_ARGS__, \
                 (code), md_error_string((code))); \
    } while (0)

/**
 * @brief Log function entry
 */
#define MD_LOG_ENTRY() \
    MD_TRACE(">> %s", __func__)

/**
 * @brief Log function exit
 */
#define MD_LOG_EXIT() \
    MD_TRACE("<< %s", __func__)

/**
 * @brief Log function entry with return value
 */
#define MD_LOG_EXIT_VAL(val) \
    MD_TRACE("<< %s = %d", __func__, (val))

/* ============================================================================
 * Conditional Logging
 * ============================================================================ */

/**
 * @brief Log only if condition is true
 */
#define MD_LOG_IF(cond, level, ...) \
    do { \
        if ((cond)) { \
            md_log_write((level), __FILE__, __LINE__, __func__, __VA_ARGS__); \
        } \
    } while (0)

/**
 * @brief Debug log only if condition is true
 */
#define MD_DEBUG_IF(cond, ...) MD_LOG_IF((cond), MD_LOG_DEBUG, __VA_ARGS__)

/**
 * @brief Trace log with hex dump
 */
#define MD_TRACE_HEXDUMP(title, data, size) \
    md_log_hexdump(MD_LOG_TRACE, __FILE__, __LINE__, __func__, \
                   (title), (data), (size))

/* ============================================================================
 * Performance Logging
 * ============================================================================ */

/**
 * @brief Performance timing context
 */
typedef struct {
    const char *name;
    long start_time;
} md_perf_timer_t;

/**
 * @brief Start a performance timer
 */
#define MD_PERF_START(name) \
    md_perf_timer_t _perf_##name = { #name, md_perf_get_time() }

/**
 * @brief End a performance timer and log
 */
#define MD_PERF_END(name) \
    MD_DEBUG("PERF %s: %ld us", _perf_##name.name, \
             md_perf_get_time() - _perf_##name.start_time)

/**
 * @brief Get current time in microseconds
 * @return Current time in microseconds
 */
MD_API long md_perf_get_time(void);

#ifdef __cplusplus
}
#endif

#endif /* MAGIC_DEBUGGER_LOGGER_H */
