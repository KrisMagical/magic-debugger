/**
 * @file gdb_adapter.h
 * @brief GDB Debugger Adapter Interface
 *
 * This file defines the GDB-specific adapter interface,
 * providing debugging capabilities through GDB MI or GDB-DAP.
 *
 * Phase 4: Adapter Layer - GDB Integration
 */

#ifndef MAGIC_DEBUGGER_GDB_ADAPTER_H
#define MAGIC_DEBUGGER_GDB_ADAPTER_H

#include "adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Default GDB executable name */
#define GDB_DEFAULT_NAME         "gdb"

/** GDB adapter name */
#define GDB_ADAPTER_NAME         "gdb"

/* ============================================================================
 * GDB Adapter Configuration
 * ============================================================================ */

/**
 * @brief GDB-specific configuration options
 */
typedef struct gdb_adapter_options {
    bool use_mi_protocol;               /**< Use MI protocol (default: true) */
    bool use_gdb_dap;                   /**< Use gdb-dap if available */
    bool print_address;                 /**< Print address in disassembly */
    char gdb_init_file[MD_MAX_PATH_LEN]; /**< GDB init file path */
    char disassembly_flavor[32];         /**< "intel" or "att" */
} gdb_adapter_options_t;

/* ============================================================================
 * GDB Adapter Creation
 * ============================================================================ */

/**
 * @brief Create GDB adapter
 * @param config Adapter configuration
 * @return Adapter handle, or NULL on error
 */
MD_API md_adapter_t* md_gdb_adapter_create(const md_adapter_config_t *config);

/**
 * @brief Initialize GDB options with defaults
 * @param options Options to initialize
 */
MD_API void gdb_adapter_options_init(gdb_adapter_options_t *options);

/* ============================================================================
 * GDB-Specific Operations
 * ============================================================================ */

/**
 * @brief Execute GDB command
 * @param adapter Adapter handle
 * @param command GDB command string
 * @param output Output buffer
 * @param output_size Buffer size
 * @return Error code
 */
MD_API md_error_t md_gdb_execute_command(md_adapter_t *adapter,
                                          const char *command,
                                          char *output,
                                          size_t output_size);

/**
 * @brief Get GDB version
 * @param adapter Adapter handle
 * @param version Buffer for version string
 * @param version_size Buffer size
 * @return Error code
 */
MD_API md_error_t md_gdb_get_version(md_adapter_t *adapter,
                                      char *version,
                                      size_t version_size);

/**
 * @brief Load GDB pretty printers
 * @param adapter Adapter handle
 * @param path Path to pretty printer
 * @return Error code
 */
MD_API md_error_t md_gdb_load_pretty_printers(md_adapter_t *adapter,
                                                const char *path);

/**
 * @brief Add GDB source path substitution
 * @param adapter Adapter handle
 * @param from Original path prefix
 * @param to Replacement path prefix
 * @return Error code
 */
MD_API md_error_t md_gdb_add_substitute_path(md_adapter_t *adapter,
                                              const char *from,
                                              const char *to);

#ifdef __cplusplus
}
#endif

#endif /* MAGIC_DEBUGGER_GDB_ADAPTER_H */
