/**
 * @file logger.c
 * @brief Implementation of the logging system
 */

#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>  /* For strcasecmp */
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal logger state
 */
typedef struct {
    bool initialized;
    md_logger_config_t config;
    FILE *log_file_handle;
    pthread_mutex_t mutex;
    int self_pipe[2];  /* For signal-safe logging */
} md_logger_state_t;

/* Global logger state */
static md_logger_state_t g_logger = {
    .initialized = false,
    .log_file_handle = NULL,
    .self_pipe = { -1, -1 },
};

/* ANSI color codes */
static const char *g_level_colors[] = {
    "\033[37m",      /* TRACE: White */
    "\033[36m",      /* DEBUG: Cyan */
    "\033[32m",      /* INFO: Green */
    "\033[33m",      /* WARNING: Yellow */
    "\033[31m",      /* ERROR: Red */
    "\033[35m",      /* FATAL: Magenta */
    "",              /* NONE */
};

static const char *g_level_names[] = {
    "TRACE",
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "FATAL",
    "NONE",
};

/* ============================================================================
 * Initialization
 * ============================================================================ */

void md_logger_config_init(md_logger_config_t *config) {
    if (config == NULL) {
        return;
    }
    
    memset(config, 0, sizeof(md_logger_config_t));
    config->level = MD_LOG_INFO;
    config->destinations = MD_LOG_DEST_DEFAULT;
    config->options = MD_LOG_OPT_DEFAULT;
    config->flush_on_write = true;
    config->quiet_mode = false;
}

md_error_t md_logger_init(const md_logger_config_t *config) {
    if (g_logger.initialized) {
        return MD_ERROR_ALREADY_INITIALIZED;
    }
    
    /* Initialize mutex */
    if (pthread_mutex_init(&g_logger.mutex, NULL) != 0) {
        return MD_ERROR_UNKNOWN;
    }
    
    /* Set configuration */
    if (config != NULL) {
        memcpy(&g_logger.config, config, sizeof(md_logger_config_t));
    } else {
        md_logger_config_init(&g_logger.config);
    }
    
    /* Open log file if needed */
    if (g_logger.config.destinations & MD_LOG_DEST_FILE) {
        if (g_logger.config.log_file[0] == '\0') {
            pthread_mutex_destroy(&g_logger.mutex);
            return MD_ERROR_INVALID_ARG;
        }
        
        g_logger.log_file_handle = fopen(g_logger.config.log_file, "a");
        if (g_logger.log_file_handle == NULL) {
            pthread_mutex_destroy(&g_logger.mutex);
            return MD_ERROR_IO_ERROR;
        }
    }
    
    g_logger.initialized = true;
    return MD_SUCCESS;
}

void md_logger_shutdown(void) {
    if (!g_logger.initialized) {
        return;
    }
    
    pthread_mutex_lock(&g_logger.mutex);
    
    if (g_logger.log_file_handle != NULL) {
        fclose(g_logger.log_file_handle);
        g_logger.log_file_handle = NULL;
    }
    
    if (g_logger.self_pipe[0] >= 0) {
        close(g_logger.self_pipe[0]);
        g_logger.self_pipe[0] = -1;
    }
    if (g_logger.self_pipe[1] >= 0) {
        close(g_logger.self_pipe[1]);
        g_logger.self_pipe[1] = -1;
    }
    
    g_logger.initialized = false;
    
    pthread_mutex_unlock(&g_logger.mutex);
    pthread_mutex_destroy(&g_logger.mutex);
}

bool md_logger_is_initialized(void) {
    return g_logger.initialized;
}

md_error_t md_logger_reconfigure(const md_logger_config_t *config) {
    if (!g_logger.initialized) {
        return md_logger_init(config);
    }
    
    pthread_mutex_lock(&g_logger.mutex);
    
    /* Close old log file */
    if (g_logger.log_file_handle != NULL) {
        fclose(g_logger.log_file_handle);
        g_logger.log_file_handle = NULL;
    }
    
    /* Apply new config */
    if (config != NULL) {
        memcpy(&g_logger.config, config, sizeof(md_logger_config_t));
    }
    
    /* Open new log file if needed */
    if (g_logger.config.destinations & MD_LOG_DEST_FILE) {
        g_logger.log_file_handle = fopen(g_logger.config.log_file, "a");
        if (g_logger.log_file_handle == NULL) {
            pthread_mutex_unlock(&g_logger.mutex);
            return MD_ERROR_IO_ERROR;
        }
    }
    
    pthread_mutex_unlock(&g_logger.mutex);
    return MD_SUCCESS;
}

md_error_t md_logger_get_config(md_logger_config_t *config) {
    if (config == NULL) {
        return MD_ERROR_NULL_POINTER;
    }
    
    pthread_mutex_lock(&g_logger.mutex);
    memcpy(config, &g_logger.config, sizeof(md_logger_config_t));
    pthread_mutex_unlock(&g_logger.mutex);
    
    return MD_SUCCESS;
}

/* ============================================================================
 * Level Management
 * ============================================================================ */

md_error_t md_logger_set_level(md_log_level_t level) {
    if (level < MD_LOG_TRACE || level > MD_LOG_NONE) {
        return MD_ERROR_INVALID_ARG;
    }
    
    pthread_mutex_lock(&g_logger.mutex);
    g_logger.config.level = level;
    pthread_mutex_unlock(&g_logger.mutex);
    
    return MD_SUCCESS;
}

md_log_level_t md_logger_get_level(void) {
    return g_logger.config.level;
}

bool md_logger_would_log(md_log_level_t level) {
    return level >= g_logger.config.level;
}

const char* md_log_level_string(md_log_level_t level) {
    if (level < MD_LOG_TRACE || level > MD_LOG_NONE) {
        return "UNKNOWN";
    }
    return g_level_names[level];
}

md_log_level_t md_log_level_from_string(const char *str) {
    if (str == NULL) {
        return MD_LOG_NONE;
    }
    
    /* Try exact match first */
    for (int i = MD_LOG_TRACE; i <= MD_LOG_NONE; i++) {
        if (strcasecmp(str, g_level_names[i]) == 0) {
            return (md_log_level_t)i;
        }
    }
    
    /* Try lowercase */
    for (int i = MD_LOG_TRACE; i <= MD_LOG_NONE; i++) {
        if (strcasecmp(str, g_level_names[i]) == 0) {
            return (md_log_level_t)i;
        }
    }
    
    return MD_LOG_NONE;
}

/* ============================================================================
 * Core Logging
 * ============================================================================ */

static void format_timestamp(char *buffer, size_t size) {
    struct timespec ts;
    struct tm tm_info;
    
    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm_info);
    
    snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
             tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday,
             tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec,
             ts.tv_nsec / 1000000);
}

static void write_to_destinations(md_log_level_t level, const char *message,
                                   const char *file, int line, const char *func) {
    char timestamp[32] = "";
    char formatted[4096];
    int offset = 0;
    md_logger_config_t *cfg = &g_logger.config;
    
    /* Build formatted message */
    if (cfg->options & MD_LOG_OPT_TIMESTAMP) {
        format_timestamp(timestamp, sizeof(timestamp));
        offset += snprintf(formatted + offset, sizeof(formatted) - offset,
                          "[%s] ", timestamp);
    }
    
    if (cfg->options & MD_LOG_OPT_PID) {
        offset += snprintf(formatted + offset, sizeof(formatted) - offset,
                          "[%d] ", getpid());
    }
    
    if (cfg->options & MD_LOG_OPT_LEVEL) {
        if (cfg->options & MD_LOG_OPT_COLOR) {
            offset += snprintf(formatted + offset, sizeof(formatted) - offset,
                              "%s%-7s\033[0m ",
                              g_level_colors[level], g_level_names[level]);
        } else {
            offset += snprintf(formatted + offset, sizeof(formatted) - offset,
                              "%-7s ", g_level_names[level]);
        }
    }
    
    if (cfg->options & MD_LOG_OPT_FILE_LINE) {
        const char *basename = strrchr(file, '/');
        basename = basename ? basename + 1 : file;
        offset += snprintf(formatted + offset, sizeof(formatted) - offset,
                          "[%s:%d] ", basename, line);
    }
    
    if (cfg->options & MD_LOG_OPT_FUNCTION) {
        offset += snprintf(formatted + offset, sizeof(formatted) - offset,
                          "[%s] ", func);
    }
    
    if (cfg->options & MD_LOG_OPT_THREAD_ID) {
        offset += snprintf(formatted + offset, sizeof(formatted) - offset,
                          "[%lu] ", (unsigned long)pthread_self());
    }
    
    offset += snprintf(formatted + offset, sizeof(formatted) - offset,
                      "%s\n", message);
    
    /* Write to destinations */
    if (!cfg->quiet_mode) {
        if (cfg->destinations & MD_LOG_DEST_STDOUT) {
            fputs(formatted, stdout);
            if (cfg->flush_on_write) {
                fflush(stdout);
            }
        }
        
        if (cfg->destinations & MD_LOG_DEST_STDERR) {
            fputs(formatted, stderr);
            if (cfg->flush_on_write) {
                fflush(stderr);
            }
        }
        
        if ((cfg->destinations & MD_LOG_DEST_FILE) && 
            g_logger.log_file_handle != NULL) {
            fputs(formatted, g_logger.log_file_handle);
            if (cfg->flush_on_write) {
                fflush(g_logger.log_file_handle);
            }
        }
    }
    
    /* Call callback if registered */
    if ((cfg->destinations & MD_LOG_DEST_CALLBACK) && 
        cfg->callback != NULL) {
        cfg->callback(level, file, line, func, message, cfg->callback_data);
    }
}

void md_log_write(md_log_level_t level, const char *file, int line,
                  const char *func, const char *format, ...) {
    if (!g_logger.initialized) {
        /* Fallback: write to stderr */
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        fprintf(stderr, "\n");
        va_end(args);
        return;
    }
    
    if (level < g_logger.config.level) {
        return;
    }
    
    char message[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    pthread_mutex_lock(&g_logger.mutex);
    write_to_destinations(level, message, file, line, func);
    pthread_mutex_unlock(&g_logger.mutex);
}

void md_log_write_v(md_log_level_t level, const char *file, int line,
                    const char *func, const char *format, va_list args) {
    if (!g_logger.initialized) {
        vfprintf(stderr, format, args);
        fprintf(stderr, "\n");
        return;
    }
    
    if (level < g_logger.config.level) {
        return;
    }
    
    char message[2048];
    vsnprintf(message, sizeof(message), format, args);
    
    pthread_mutex_lock(&g_logger.mutex);
    write_to_destinations(level, message, file, line, func);
    pthread_mutex_unlock(&g_logger.mutex);
}

void md_log_hexdump(md_log_level_t level, const char *file, int line,
                    const char *func, const char *title,
                    const void *data, size_t size) {
    if (!g_logger.initialized || level < g_logger.config.level) {
        return;
    }
    
    const unsigned char *bytes = (const unsigned char *)data;
    char line_buf[128];
    int offset;
    
    pthread_mutex_lock(&g_logger.mutex);
    
    /* Write title */
    if (title != NULL) {
        write_to_destinations(level, title, file, line, func);
    }
    
    /* Write hex dump */
    for (size_t i = 0; i < size; i += 16) {
        offset = 0;
        
        /* Address */
        offset += snprintf(line_buf + offset, sizeof(line_buf) - offset,
                          "%04zx: ", i);
        
        /* Hex bytes */
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                offset += snprintf(line_buf + offset, sizeof(line_buf) - offset,
                                  "%02x ", bytes[i + j]);
            } else {
                offset += snprintf(line_buf + offset, sizeof(line_buf) - offset,
                                  "   ");
            }
            if (j == 7) {
                offset += snprintf(line_buf + offset, sizeof(line_buf) - offset, " ");
            }
        }
        
        offset += snprintf(line_buf + offset, sizeof(line_buf) - offset, " |");
        
        /* ASCII */
        for (size_t j = 0; j < 16 && i + j < size; j++) {
            unsigned char c = bytes[i + j];
            line_buf[offset++] = (c >= 32 && c < 127) ? c : '.';
        }
        
        offset += snprintf(line_buf + offset, sizeof(line_buf) - offset, "|");
        
        write_to_destinations(level, line_buf, file, line, func);
    }
    
    pthread_mutex_unlock(&g_logger.mutex);
}

/* ============================================================================
 * Performance Timing
 * ============================================================================ */

long md_perf_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}
