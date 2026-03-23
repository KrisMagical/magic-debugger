/**
 * @file shell_adapter.h
 * @brief Shell Debugger Adapter Interface
 *
 * This file defines the Shell-specific adapter interface,
 * providing debugging capabilities through bashdb.
 *
 * Phase 4: Adapter Layer - Shell Integration
 */

#ifndef MAGIC_DEBUGGER_SHELL_ADAPTER_H
#define MAGIC_DEBUGGER_SHELL_ADAPTER_H

#include "adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Default bashdb executable name */
#define SHELL_ADAPTER_DEFAULT_NAME    "bashdb"

/** Shell adapter name */
#define SHELL_ADAPTER_NAME            "shell"

/* ============================================================================
 * Shell Adapter Configuration
 * ============================================================================ */

/**
 * @brief Shell-specific configuration options
 */
typedef struct shell_adapter_options {
    bool debug_mode;                    /**< Enable debug mode */
    bool line_numbers;                  /**< Show line numbers in output */
    char shell_path[MD_MAX_PATH_LEN];   /**< Path to shell interpreter */
    char rc_file[MD_MAX_PATH_LEN];      /**< RC file to source */
} shell_adapter_options_t;

/* ============================================================================
 * Shell Adapter Creation
 * ============================================================================ */

/**
 * @brief Create Shell adapter
 * @param config Adapter configuration
 * @return Adapter handle, or NULL on error
 */
MD_API md_adapter_t* md_shell_adapter_create(const md_adapter_config_t *config);

/**
 * @brief Initialize shell options with defaults
 * @param options Options to initialize
 */
MD_API void shell_adapter_options_init(shell_adapter_options_t *options);

/* ============================================================================
 * Shell-Specific Operations
 * ============================================================================ */

/**
 * @brief Execute shell command
 * @param adapter Adapter handle
 * @param command Shell command
 * @param output Output buffer
 * @param output_size Buffer size
 * @return Error code
 */
MD_API md_error_t md_shell_execute_command(md_adapter_t *adapter,
                                            const char *command,
                                            char *output,
                                            size_t output_size);

/**
 * @brief Get current line number
 * @param adapter Adapter handle
 * @return Current line number
 */
MD_API int md_shell_get_current_line(md_adapter_t *adapter);

/**
 * @brief Get current script path
 * @param adapter Adapter handle
 * @param path Buffer for path
 * @param path_size Buffer size
 * @return Error code
 */
MD_API md_error_t md_shell_get_current_script(md_adapter_t *adapter,
                                               char *path,
                                               size_t path_size);

/**
 * @brief Check if at breakpoint
 * @param adapter Adapter handle
 * @return true if stopped at breakpoint
 */
MD_API bool md_shell_is_at_breakpoint(md_adapter_t *adapter);

#ifdef __cplusplus
}
#endif

#endif /* MAGIC_DEBUGGER_SHELL_ADAPTER_H */
