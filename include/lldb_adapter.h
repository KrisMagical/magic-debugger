/**
 * @file lldb_adapter.h
 * @brief LLDB Debugger Adapter Interface
 *
 * This file defines the LLDB-specific adapter interface,
 * providing debugging capabilities through lldb-dap.
 *
 * Phase 4: Adapter Layer - LLDB Integration
 */

#ifndef MAGIC_DEBUGGER_LLDB_ADAPTER_H
#define MAGIC_DEBUGGER_LLDB_ADAPTER_H

#include "adapter.h"
#include "dap_client.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Default LLDB-DAP executable name */
#define LLDB_DAP_DEFAULT_NAME    "lldb-dap"

/** Default LLDB-DAP paths to search */
#define LLDB_DAP_PATHS \
    "/usr/bin/lldb-dap", \
    "/usr/local/bin/lldb-dap", \
    "/opt/homebrew/bin/lldb-dap", \
    "/Applications/Xcode.app/Contents/Developer/usr/bin/lldb-dap"

/** LLDB adapter name */
#define LLDB_ADAPTER_NAME        "lldb"

/* ============================================================================
 * LLDB Adapter Configuration
 * ============================================================================ */

/**
 * @brief LLDB-specific configuration options
 */
typedef struct lldb_adapter_options {
    bool display_runtime_support_values;  /**< Show runtime support values */
    bool command_completions_enabled;      /**< Enable command completions */
    bool supports_granular_stepping;       /**< Use granular stepping */
    char init_commands[MD_MAX_PATH_LEN];   /**< Commands to run on init */
    char pre_run_commands[MD_MAX_PATH_LEN]; /**< Commands before launch */
} lldb_adapter_options_t;

/* ============================================================================
 * LLDB Adapter Creation
 * ============================================================================ */

/**
 * @brief Create LLDB adapter
 * @param config Adapter configuration
 * @return Adapter handle, or NULL on error
 */
MD_API md_adapter_t* md_lldb_adapter_create(const md_adapter_config_t *config);

/**
 * @brief Initialize LLDB options with defaults
 * @param options Options to initialize
 */
MD_API void lldb_adapter_options_init(lldb_adapter_options_t *options);

/* ============================================================================
 * LLDB-Specific Operations
 * ============================================================================ */

/**
 * @brief Execute LLDB command
 * @param adapter Adapter handle
 * @param command LLDB command string
 * @param output Output buffer
 * @param output_size Buffer size
 * @return Error code
 */
MD_API md_error_t md_lldb_execute_command(md_adapter_t *adapter,
                                           const char *command,
                                           char *output,
                                           size_t output_size);

/**
 * @brief Get LLDB version
 * @param adapter Adapter handle
 * @param version Buffer for version string
 * @param version_size Buffer size
 * @return Error code
 */
MD_API md_error_t md_lldb_get_version(md_adapter_t *adapter,
                                       char *version,
                                       size_t version_size);

/**
 * @brief Load symbols for a module
 * @param adapter Adapter handle
 * @param module_path Path to module
 * @param symbol_path Path to symbol file (optional)
 * @return Error code
 */
MD_API md_error_t md_lldb_load_symbols(md_adapter_t *adapter,
                                        const char *module_path,
                                        const char *symbol_path);

/**
 * @brief Set disassembly flavor
 * @param adapter Adapter handle
 * @param flavor "intel" or "att"
 * @return Error code
 */
MD_API md_error_t md_lldb_set_disassembly_flavor(md_adapter_t *adapter,
                                                  const char *flavor);

#ifdef __cplusplus
}
#endif

#endif /* MAGIC_DEBUGGER_LLDB_ADAPTER_H */
