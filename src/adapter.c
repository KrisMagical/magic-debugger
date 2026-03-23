/**
 * @file adapter.c
 * @brief Implementation of Unified Debugger Adapter Interface
 *
 * This file provides the base implementation for debugger adapters,
 * with support for LLDB, GDB, and Shell debuggers.
 *
 * Phase 4: Adapter Layer - Debugger Integration
 */

#include "adapter.h"
#include "logger.h"
#include "session_manager.h"
#include "dap_client.h"
#include "state_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* ============================================================================
 * Adapter Registry
 * ============================================================================ */

#define MD_MAX_ADAPTER_TYPES 8

typedef struct {
    md_debugger_type_t type;
    md_adapter_factory_t factory;
    char name[32];
} md_adapter_registry_entry_t;

static md_adapter_registry_entry_t g_registry[MD_MAX_ADAPTER_TYPES];
static int g_registry_count = 0;
static bool g_initialized = false;

/* Forward declarations for built-in adapters */
extern md_adapter_t* md_lldb_adapter_create(const md_adapter_config_t *config);
extern md_adapter_t* md_gdb_adapter_create(const md_adapter_config_t *config);
extern md_adapter_t* md_shell_adapter_create(const md_adapter_config_t *config);

/* ============================================================================
 * Subsystem Initialization
 * ============================================================================ */

static void register_builtin_adapters(void) {
    if (g_initialized) return;
    
    /* Register LLDB adapter */
    md_adapter_register(MD_DEBUGGER_LLDB, md_lldb_adapter_create);
    
    /* Register GDB adapter */
    md_adapter_register(MD_DEBUGGER_GDB, md_gdb_adapter_create);
    
    /* Register Shell adapter */
    md_adapter_register(MD_DEBUGGER_SHELL, md_shell_adapter_create);
    
    g_initialized = true;
}

md_error_t md_adapter_register(md_debugger_type_t type,
                                md_adapter_factory_t factory) {
    if (g_registry_count >= MD_MAX_ADAPTER_TYPES) {
        return MD_ERROR_BUFFER_OVERFLOW;
    }
    
    g_registry[g_registry_count].type = type;
    g_registry[g_registry_count].factory = factory;
    
    const char *name = "unknown";
    switch (type) {
        case MD_DEBUGGER_LLDB: name = "lldb"; break;
        case MD_DEBUGGER_GDB: name = "gdb"; break;
        case MD_DEBUGGER_SHELL: name = "shell"; break;
        default: break;
    }
    
    strncpy(g_registry[g_registry_count].name, name, sizeof(g_registry[0].name) - 1);
    g_registry_count++;
    
    MD_DEBUG("Registered adapter: %s", name);
    return MD_SUCCESS;
}

void md_adapter_unregister(md_debugger_type_t type) {
    for (int i = 0; i < g_registry_count; i++) {
        if (g_registry[i].type == type) {
            for (int j = i; j < g_registry_count - 1; j++) {
                g_registry[j] = g_registry[j + 1];
            }
            g_registry_count--;
            return;
        }
    }
}

md_adapter_t* md_adapter_create(md_debugger_type_t type,
                                 const md_adapter_config_t *config) {
    register_builtin_adapters();
    
    for (int i = 0; i < g_registry_count; i++) {
        if (g_registry[i].type == type) {
            if (g_registry[i].factory == NULL) {
                MD_ERROR("No factory registered for type %d", type);
                return NULL;
            }
            return g_registry[i].factory(config);
        }
    }
    
    MD_ERROR("Unknown adapter type: %d", type);
    return NULL;
}

md_adapter_t* md_adapter_create_by_name(const char *name,
                                         const md_adapter_config_t *config) {
    if (name == NULL) return NULL;
    
    md_debugger_type_t type = MD_DEBUGGER_CUSTOM;
    
    if (strcmp(name, "lldb") == 0 || strcmp(name, "lldb-dap") == 0) {
        type = MD_DEBUGGER_LLDB;
    } else if (strcmp(name, "gdb") == 0 || strcmp(name, "gdb-mi") == 0 || 
               strcmp(name, "gdb-dap") == 0) {
        type = MD_DEBUGGER_GDB;
    } else if (strcmp(name, "shell") == 0 || strcmp(name, "bashdb") == 0) {
        type = MD_DEBUGGER_SHELL;
    }
    
    return md_adapter_create(type, config);
}

void md_adapter_destroy(md_adapter_t *adapter) {
    if (adapter == NULL) return;
    
    if (adapter->vtable != NULL && adapter->vtable->destroy != NULL) {
        adapter->vtable->destroy(adapter);
    }
    
    /* Free state model if we own it */
    if (adapter->state_model != NULL) {
        md_state_model_destroy(adapter->state_model);
    }
    
    /* Free DAP client if we own it */
    if (adapter->dap_client != NULL) {
        dap_client_destroy(adapter->dap_client);
    }
    
    /* Free session if we own it */
    if (adapter->session != NULL) {
        md_session_destroy(adapter->session);
    }
    
    free(adapter);
    MD_DEBUG("Adapter destroyed");
}

/* ============================================================================
 * Configuration Helpers
 * ============================================================================ */

void md_adapter_config_init(md_adapter_config_t *config) {
    if (config == NULL) return;
    memset(config, 0, sizeof(md_adapter_config_t));
    config->stop_on_entry = false;
    config->auto_disassemble = true;
    config->source_context_lines = MD_ADAPTER_SOURCE_CONTEXT;
    config->timeout_ms = 10000;
    config->verbosity = 0;
}

md_error_t md_adapter_config_set_program(md_adapter_config_t *config,
                                          const char *path) {
    if (config == NULL || path == NULL) return MD_ERROR_INVALID_ARG;
    
    strncpy(config->program_path, path, MD_MAX_PATH_LEN - 1);
    return MD_SUCCESS;
}

md_error_t md_adapter_config_set_args(md_adapter_config_t *config,
                                       const char *args) {
    if (config == NULL) return MD_ERROR_INVALID_ARG;
    
    if (args != NULL) {
        strncpy(config->program_args, args, MD_MAX_PATH_LEN - 1);
    } else {
        config->program_args[0] = '\0';
    }
    return MD_SUCCESS;
}

md_error_t md_adapter_config_set_cwd(md_adapter_config_t *config,
                                      const char *dir) {
    if (config == NULL) return MD_ERROR_INVALID_ARG;
    
    if (dir != NULL) {
        strncpy(config->working_dir, dir, MD_MAX_PATH_LEN - 1);
    } else {
        config->working_dir[0] = '\0';
    }
    return MD_SUCCESS;
}

md_error_t md_adapter_config_set_debugger(md_adapter_config_t *config,
                                           const char *path) {
    if (config == NULL || path == NULL) return MD_ERROR_INVALID_ARG;
    
    strncpy(config->debugger_path, path, MD_MAX_PATH_LEN - 1);
    return MD_SUCCESS;
}

/* ============================================================================
 * Lifecycle Operations
 * ============================================================================ */

md_error_t md_adapter_init(md_adapter_t *adapter) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    if (adapter->vtable != NULL && adapter->vtable->init != NULL) {
        return adapter->vtable->init(adapter);
    }
    
    return MD_ERROR_NOT_INITIALIZED;
}

md_error_t md_adapter_launch(md_adapter_t *adapter) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    if (adapter->vtable != NULL && adapter->vtable->launch != NULL) {
        return adapter->vtable->launch(adapter);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_attach(md_adapter_t *adapter, int pid) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    if (adapter->vtable != NULL && adapter->vtable->attach != NULL) {
        return adapter->vtable->attach(adapter, pid);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_disconnect(md_adapter_t *adapter,
                                  bool terminate_debuggee) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    if (adapter->vtable != NULL && adapter->vtable->disconnect != NULL) {
        return adapter->vtable->disconnect(adapter, terminate_debuggee);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

/* ============================================================================
 * Breakpoint Operations
 * ============================================================================ */

md_error_t md_adapter_set_breakpoint(md_adapter_t *adapter,
                                      const char *path, int line,
                                      const char *condition, int *bp_id) {
    if (adapter == NULL || path == NULL || line < 1) {
        return MD_ERROR_INVALID_ARG;
    }
    
    if (adapter->vtable != NULL && adapter->vtable->set_breakpoint != NULL) {
        return adapter->vtable->set_breakpoint(adapter, path, line, condition, bp_id);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_set_function_breakpoint(md_adapter_t *adapter,
                                               const char *function,
                                               int *bp_id) {
    if (adapter == NULL || function == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    if (adapter->vtable != NULL && adapter->vtable->set_function_breakpoint != NULL) {
        return adapter->vtable->set_function_breakpoint(adapter, function, bp_id);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_remove_breakpoint(md_adapter_t *adapter, int bp_id) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    if (adapter->vtable != NULL && adapter->vtable->remove_breakpoint != NULL) {
        return adapter->vtable->remove_breakpoint(adapter, bp_id);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_enable_breakpoint(md_adapter_t *adapter,
                                         int bp_id, bool enable) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    if (adapter->vtable != NULL && adapter->vtable->enable_breakpoint != NULL) {
        return adapter->vtable->enable_breakpoint(adapter, bp_id, enable);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_set_breakpoint_condition(md_adapter_t *adapter,
                                                int bp_id,
                                                const char *condition) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    if (adapter->vtable != NULL && adapter->vtable->set_breakpoint_condition != NULL) {
        return adapter->vtable->set_breakpoint_condition(adapter, bp_id, condition);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_set_breakpoints(md_adapter_t *adapter,
                                       const char *path,
                                       const int *lines, int count,
                                       int *bp_ids) {
    if (adapter == NULL || path == NULL || lines == NULL || count < 1) {
        return MD_ERROR_INVALID_ARG;
    }
    
    md_error_t err = MD_SUCCESS;
    
    for (int i = 0; i < count; i++) {
        int bp_id = MD_INVALID_ID;
        err = md_adapter_set_breakpoint(adapter, path, lines[i], NULL, &bp_id);
        if (err != MD_SUCCESS) {
            MD_WARNING("Failed to set breakpoint at %s:%d", path, lines[i]);
            if (bp_ids != NULL) {
                bp_ids[i] = MD_INVALID_ID;
            }
        } else if (bp_ids != NULL) {
            bp_ids[i] = bp_id;
        }
    }
    
    return err;
}

md_error_t md_adapter_clear_all_breakpoints(md_adapter_t *adapter) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    md_state_model_t *model = md_adapter_get_state_model(adapter);
    if (model != NULL) {
        return md_state_clear_all_breakpoints(model);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

/* ============================================================================
 * Execution Control
 * ============================================================================ */

md_error_t md_adapter_continue(md_adapter_t *adapter) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    if (adapter->vtable != NULL && adapter->vtable->continue_exec != NULL) {
        md_error_t err = adapter->vtable->continue_exec(adapter);
        if (err == MD_SUCCESS && adapter->state_model != NULL) {
            md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_RUNNING);
        }
        return err;
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_pause(md_adapter_t *adapter) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    if (adapter->vtable != NULL && adapter->vtable->pause != NULL) {
        return adapter->vtable->pause(adapter);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_step_over(md_adapter_t *adapter,
                                 md_step_granularity_t granularity) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    if (adapter->vtable != NULL && adapter->vtable->step_over != NULL) {
        md_error_t err = adapter->vtable->step_over(adapter, granularity);
        if (err == MD_SUCCESS && adapter->state_model != NULL) {
            md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_STEPPING);
        }
        return err;
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_step_into(md_adapter_t *adapter,
                                 md_step_granularity_t granularity) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    if (adapter->vtable != NULL && adapter->vtable->step_into != NULL) {
        md_error_t err = adapter->vtable->step_into(adapter, granularity);
        if (err == MD_SUCCESS && adapter->state_model != NULL) {
            md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_STEPPING);
        }
        return err;
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_step_out(md_adapter_t *adapter) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    if (adapter->vtable != NULL && adapter->vtable->step_out != NULL) {
        md_error_t err = adapter->vtable->step_out(adapter);
        if (err == MD_SUCCESS && adapter->state_model != NULL) {
            md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_STEPPING);
        }
        return err;
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_restart(md_adapter_t *adapter) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    if (adapter->vtable != NULL && adapter->vtable->restart != NULL) {
        return adapter->vtable->restart(adapter);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_run_to_location(md_adapter_t *adapter,
                                       const char *path, int line) {
    if (adapter == NULL || path == NULL || line < 1) {
        return MD_ERROR_INVALID_ARG;
    }
    
    /* Set temporary breakpoint and continue */
    int bp_id;
    md_error_t err = md_adapter_set_breakpoint(adapter, path, line, NULL, &bp_id);
    if (err != MD_SUCCESS) return err;
    
    /* Continue execution */
    err = md_adapter_continue(adapter);
    if (err != MD_SUCCESS) {
        md_adapter_remove_breakpoint(adapter, bp_id);
        return err;
    }
    
    /* Note: Breakpoint will be hit and handled by event processing */
    return MD_SUCCESS;
}

/* ============================================================================
 * Variable Operations
 * ============================================================================ */

md_error_t md_adapter_evaluate(md_adapter_t *adapter,
                                const char *expression,
                                int frame_id,
                                md_eval_result_t *result) {
    if (adapter == NULL || expression == NULL || result == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    memset(result, 0, sizeof(md_eval_result_t));
    
    if (adapter->vtable != NULL && adapter->vtable->evaluate != NULL) {
        return adapter->vtable->evaluate(adapter, expression, frame_id, result);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_set_variable(md_adapter_t *adapter,
                                    int var_ref,
                                    const char *name,
                                    const char *value) {
    if (adapter == NULL || name == NULL || value == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    if (adapter->vtable != NULL && adapter->vtable->set_variable != NULL) {
        return adapter->vtable->set_variable(adapter, var_ref, name, value);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_get_variables(md_adapter_t *adapter,
                                     int var_ref,
                                     md_variable_t *variables,
                                     int max_count,
                                     int *actual_count) {
    if (adapter == NULL || variables == NULL || actual_count == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    *actual_count = 0;
    
    if (adapter->vtable != NULL && adapter->vtable->get_variables != NULL) {
        return adapter->vtable->get_variables(adapter, var_ref, variables, 
                                               max_count, actual_count);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_get_scopes(md_adapter_t *adapter,
                                  int frame_id,
                                  md_scope_t *scopes,
                                  int max_count,
                                  int *actual_count) {
    if (adapter == NULL || scopes == NULL || actual_count == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    *actual_count = 0;
    
    if (adapter->vtable != NULL && adapter->vtable->get_scopes != NULL) {
        return adapter->vtable->get_scopes(adapter, frame_id, scopes,
                                            max_count, actual_count);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_print_variable(md_adapter_t *adapter,
                                      const char *var_name,
                                      char *output,
                                      size_t output_size) {
    if (adapter == NULL || var_name == NULL || output == NULL || output_size == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    int frame_id = md_adapter_get_current_frame_id(adapter);
    md_eval_result_t result;
    
    md_error_t err = md_adapter_evaluate(adapter, var_name, frame_id, &result);
    if (err != MD_SUCCESS) {
        snprintf(output, output_size, "Error: %s", result.error);
        return err;
    }
    
    if (result.type[0] != '\0') {
        snprintf(output, output_size, "%s = %s (%s)", var_name, result.value, result.type);
    } else {
        snprintf(output, output_size, "%s = %s", var_name, result.value);
    }
    
    return MD_SUCCESS;
}

/* ============================================================================
 * Stack and Thread Operations
 * ============================================================================ */

md_error_t md_adapter_get_threads(md_adapter_t *adapter,
                                   md_thread_t *threads,
                                   int max_count,
                                   int *actual_count) {
    if (adapter == NULL || threads == NULL || actual_count == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    *actual_count = 0;
    
    if (adapter->vtable != NULL && adapter->vtable->get_threads != NULL) {
        return adapter->vtable->get_threads(adapter, threads, max_count, actual_count);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_get_stack_frames(md_adapter_t *adapter,
                                        int thread_id,
                                        md_stack_frame_t *frames,
                                        int max_count,
                                        int *actual_count) {
    if (adapter == NULL || frames == NULL || actual_count == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    *actual_count = 0;
    
    if (adapter->vtable != NULL && adapter->vtable->get_stack_frames != NULL) {
        return adapter->vtable->get_stack_frames(adapter, thread_id, frames,
                                                  max_count, actual_count);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_set_active_thread(md_adapter_t *adapter,
                                         int thread_id) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    if (adapter->vtable != NULL && adapter->vtable->set_active_thread != NULL) {
        md_error_t err = adapter->vtable->set_active_thread(adapter, thread_id);
        if (err == MD_SUCCESS && adapter->state_model != NULL) {
            md_state_set_current_thread_id(adapter->state_model, thread_id);
        }
        return err;
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_set_active_frame(md_adapter_t *adapter,
                                        int frame_id) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    if (adapter->vtable != NULL && adapter->vtable->set_active_frame != NULL) {
        md_error_t err = adapter->vtable->set_active_frame(adapter, frame_id);
        if (err == MD_SUCCESS && adapter->state_model != NULL) {
            md_state_set_current_frame_id(adapter->state_model, frame_id);
        }
        return err;
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

int md_adapter_get_current_thread_id(md_adapter_t *adapter) {
    if (adapter == NULL) return MD_INVALID_ID;
    
    if (adapter->state_model != NULL) {
        return md_state_get_current_thread_id(adapter->state_model);
    }
    
    return MD_INVALID_ID;
}

int md_adapter_get_current_frame_id(md_adapter_t *adapter) {
    if (adapter == NULL) return MD_INVALID_ID;
    
    if (adapter->state_model != NULL) {
        return md_state_get_current_frame_id(adapter->state_model);
    }
    
    return MD_INVALID_ID;
}

/* ============================================================================
 * Source Code Display
 * ============================================================================ */

md_error_t md_adapter_get_source(md_adapter_t *adapter,
                                  const char *path,
                                  int start_line,
                                  int end_line,
                                  md_source_context_t *ctx) {
    if (adapter == NULL || path == NULL || ctx == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    if (adapter->vtable != NULL && adapter->vtable->get_source != NULL) {
        return adapter->vtable->get_source(adapter, path, start_line, end_line, ctx);
    }
    
    /* Default implementation: read file directly */
    memset(ctx, 0, sizeof(md_source_context_t));
    strncpy(ctx->path, path, MD_MAX_PATH_LEN - 1);
    
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        MD_ERROR("Failed to open source file: %s", path);
        return MD_ERROR_IO_ERROR;
    }
    
    /* Count lines and find current position */
    int total_lines = 0;
    char line_buf[MD_ADAPTER_LINE_MAX];
    while (fgets(line_buf, sizeof(line_buf), fp) != NULL) {
        total_lines++;
    }
    ctx->total_lines = total_lines;
    
    /* Calculate context range */
    if (start_line <= 0 || end_line <= 0) {
        int current = md_state_get_current_thread_id(adapter->state_model) >= 0 ?
                      /* Try to get current line from stack frame */ 1 : 1;
        ctx->current_line = current;
        ctx->start_line = current - adapter->config.source_context_lines;
        ctx->end_line = current + adapter->config.source_context_lines;
    } else {
        ctx->start_line = start_line;
        ctx->end_line = end_line;
    }
    
    /* Clamp to valid range */
    if (ctx->start_line < 1) ctx->start_line = 1;
    if (ctx->end_line > total_lines) ctx->end_line = total_lines;
    
    /* Allocate lines */
    int line_count = ctx->end_line - ctx->start_line + 1;
    ctx->lines = (md_source_line_t*)calloc(line_count, sizeof(md_source_line_t));
    if (ctx->lines == NULL) {
        fclose(fp);
        return MD_ERROR_OUT_OF_MEMORY;
    }
    ctx->line_count = line_count;
    
    /* Read lines */
    rewind(fp);
    int line_num = 0;
    int idx = 0;
    while (fgets(line_buf, sizeof(line_buf), fp) != NULL && line_num < ctx->end_line) {
        line_num++;
        if (line_num >= ctx->start_line && line_num <= ctx->end_line) {
            ctx->lines[idx].line_number = line_num;
            strncpy(ctx->lines[idx].content, line_buf, MD_ADAPTER_LINE_MAX - 1);
            /* Remove trailing newline */
            size_t len = strlen(ctx->lines[idx].content);
            if (len > 0 && ctx->lines[idx].content[len - 1] == '\n') {
                ctx->lines[idx].content[len - 1] = '\0';
            }
            ctx->lines[idx].is_executable = true; /* Simplified */
            ctx->lines[idx].is_current = (line_num == ctx->current_line);
            
            /* Check for breakpoint */
            if (adapter->state_model != NULL) {
                md_breakpoint_t bp;
                if (md_state_find_breakpoint_by_location(adapter->state_model, 
                                                         path, line_num, &bp) == MD_SUCCESS) {
                    ctx->lines[idx].has_breakpoint = true;
                    ctx->lines[idx].breakpoint_id = bp.id;
                    strcpy(ctx->lines[idx].breakpoint_marker, bp.enabled ? "B" : "b");
                }
            }
            
            /* Set markers */
            if (ctx->lines[idx].is_current) {
                strcpy(ctx->lines[idx].breakpoint_marker, ">");
            } else if (ctx->lines[idx].has_breakpoint) {
                /* Already set above */
            } else {
                strcpy(ctx->lines[idx].breakpoint_marker, " ");
            }
            
            idx++;
        }
    }
    
    fclose(fp);
    return MD_SUCCESS;
}

void md_adapter_free_source_context(md_source_context_t *ctx) {
    if (ctx == NULL) return;
    
    if (ctx->lines != NULL) {
        free(ctx->lines);
        ctx->lines = NULL;
    }
    ctx->line_count = 0;
}

md_error_t md_adapter_disassemble(md_adapter_t *adapter,
                                   uint64_t address,
                                   char *output,
                                   size_t output_size) {
    if (adapter == NULL || output == NULL || output_size == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    if (adapter->vtable != NULL && adapter->vtable->disassemble != NULL) {
        return adapter->vtable->disassemble(adapter, address, output, output_size);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_format_source(md_adapter_t *adapter,
                                     const md_source_context_t *ctx,
                                     char *output,
                                     size_t output_size) {
    if (adapter == NULL || ctx == NULL || output == NULL || output_size == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    size_t offset = 0;
    int ret;
    
    /* Header */
    ret = snprintf(output + offset, output_size - offset,
                   "File: %s (lines %d-%d of %d)\n",
                   ctx->path, ctx->start_line, ctx->end_line, ctx->total_lines);
    if (ret < 0 || (size_t)ret >= output_size - offset) {
        return MD_ERROR_BUFFER_TOO_SMALL;
    }
    offset += ret;
    
    /* Separator */
    ret = snprintf(output + offset, output_size - offset,
                   "    Line  M  Content\n"
                   "    ----  -  -------\n");
    if (ret < 0 || (size_t)ret >= output_size - offset) {
        return MD_ERROR_BUFFER_TOO_SMALL;
    }
    offset += ret;
    
    /* Lines */
    for (int i = 0; i < ctx->line_count && offset < output_size - 1; i++) {
        ret = snprintf(output + offset, output_size - offset,
                       "    %4d  %s  %s\n",
                       ctx->lines[i].line_number,
                       ctx->lines[i].breakpoint_marker,
                       ctx->lines[i].content);
        if (ret < 0) break;
        offset += ret;
    }
    
    return MD_SUCCESS;
}

/* ============================================================================
 * State Query Operations
 * ============================================================================ */

md_adapter_state_t md_adapter_get_state(md_adapter_t *adapter) {
    if (adapter == NULL) return MD_ADAPTER_STATE_NONE;
    
    if (adapter->vtable != NULL && adapter->vtable->get_state != NULL) {
        return adapter->vtable->get_state(adapter);
    }
    
    return adapter->state;
}

const char* md_adapter_state_string(md_adapter_state_t state) {
    switch (state) {
        case MD_ADAPTER_STATE_NONE:       return "none";
        case MD_ADAPTER_STATE_INITIALIZED: return "initialized";
        case MD_ADAPTER_STATE_LAUNCHING:  return "launching";
        case MD_ADAPTER_STATE_RUNNING:    return "running";
        case MD_ADAPTER_STATE_STOPPED:    return "stopped";
        case MD_ADAPTER_STATE_TERMINATED: return "terminated";
        case MD_ADAPTER_STATE_ERROR:      return "error";
        default:                          return "unknown";
    }
}

md_error_t md_adapter_get_exception(md_adapter_t *adapter,
                                     md_exception_t *exception) {
    if (adapter == NULL || exception == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    if (adapter->vtable != NULL && adapter->vtable->get_exception != NULL) {
        return adapter->vtable->get_exception(adapter, exception);
    }
    
    if (adapter->state_model != NULL) {
        return md_state_get_exception(adapter->state_model, exception);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

md_error_t md_adapter_get_debug_state(md_adapter_t *adapter,
                                       md_debug_state_t *state) {
    if (adapter == NULL || state == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    if (adapter->vtable != NULL && adapter->vtable->get_debug_state != NULL) {
        return adapter->vtable->get_debug_state(adapter, state);
    }
    
    if (adapter->state_model != NULL) {
        return md_state_get_debug_state(adapter->state_model, state);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

struct md_state_model* md_adapter_get_state_model(md_adapter_t *adapter) {
    return adapter != NULL ? adapter->state_model : NULL;
}

/* ============================================================================
 * Event Processing
 * ============================================================================ */

md_error_t md_adapter_process_events(md_adapter_t *adapter,
                                      int timeout_ms) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    if (adapter->vtable != NULL && adapter->vtable->process_events != NULL) {
        return adapter->vtable->process_events(adapter, timeout_ms);
    }
    
    return MD_SUCCESS;
}

md_error_t md_adapter_wait_for_stop(md_adapter_t *adapter,
                                     int timeout_ms,
                                     md_stop_reason_t *reason) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    int elapsed = 0;
    int poll_interval = 50;
    
    while (elapsed < timeout_ms) {
        md_error_t err = md_adapter_process_events(adapter, poll_interval);
        if (err != MD_SUCCESS && err != MD_ERROR_TIMEOUT) {
            return err;
        }
        
        md_adapter_state_t state = md_adapter_get_state(adapter);
        if (state == MD_ADAPTER_STATE_STOPPED) {
            if (reason != NULL && adapter->state_model != NULL) {
                *reason = md_state_get_stop_reason(adapter->state_model);
            }
            return MD_SUCCESS;
        }
        
        if (state == MD_ADAPTER_STATE_TERMINATED) {
            if (reason != NULL) {
                *reason = MD_STOP_REASON_UNKNOWN;
            }
            return MD_SUCCESS;
        }
        
        elapsed += poll_interval;
    }
    
    return MD_ERROR_TIMEOUT;
}

/* ============================================================================
 * Capabilities
 * ============================================================================ */

bool md_adapter_supports_feature(md_adapter_t *adapter,
                                  const char *feature) {
    if (adapter == NULL || feature == NULL) return false;
    
    if (adapter->vtable != NULL && adapter->vtable->supports_feature != NULL) {
        return adapter->vtable->supports_feature(adapter, feature);
    }
    
    /* Check capabilities directly */
    if (!adapter->capabilities_loaded) return false;
    
    if (strcmp(feature, "conditionalBreakpoints") == 0) {
        return adapter->capabilities.supports_conditional_breakpoints;
    }
    if (strcmp(feature, "hitConditionalBreakpoints") == 0) {
        return adapter->capabilities.supports_hit_conditional_breakpoints;
    }
    if (strcmp(feature, "functionBreakpoints") == 0) {
        return adapter->capabilities.supports_function_breakpoints;
    }
    if (strcmp(feature, "logPoints") == 0) {
        return adapter->capabilities.supports_log_points;
    }
    if (strcmp(feature, "dataBreakpoints") == 0) {
        return adapter->capabilities.supports_data_breakpoints;
    }
    if (strcmp(feature, "stepBack") == 0) {
        return adapter->capabilities.supports_step_back;
    }
    if (strcmp(feature, "setVariable") == 0) {
        return adapter->capabilities.supports_set_variable;
    }
    
    return false;
}

const char* md_adapter_get_name(md_adapter_t *adapter) {
    if (adapter == NULL) return "null";
    
    if (adapter->vtable != NULL && adapter->vtable->get_name != NULL) {
        return adapter->vtable->get_name(adapter);
    }
    
    return adapter->name;
}

const char* md_adapter_get_version(md_adapter_t *adapter) {
    if (adapter == NULL) return "0.0";
    
    if (adapter->vtable != NULL && adapter->vtable->get_version != NULL) {
        return adapter->vtable->get_version(adapter);
    }
    
    return adapter->version;
}

const dap_capabilities_t* md_adapter_get_capabilities(md_adapter_t *adapter) {
    if (adapter == NULL || !adapter->capabilities_loaded) {
        return NULL;
    }
    return &adapter->capabilities;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* md_step_granularity_string(md_step_granularity_t granularity) {
    switch (granularity) {
        case MD_STEP_LINE:        return "line";
        case MD_STEP_STATEMENT:   return "statement";
        case MD_STEP_INSTRUCTION: return "instruction";
        default:                  return "unknown";
    }
}

md_debugger_type_t md_adapter_detect_type(const char *path) {
    if (path == NULL) return MD_DEBUGGER_NONE;
    
    /* Extract basename */
    const char *basename = strrchr(path, '/');
    if (basename != NULL) {
        basename++;
    } else {
        basename = path;
    }
    
    if (strstr(basename, "lldb") != NULL || strstr(basename, "lldb-dap") != NULL) {
        return MD_DEBUGGER_LLDB;
    }
    if (strstr(basename, "gdb") != NULL) {
        return MD_DEBUGGER_GDB;
    }
    if (strstr(basename, "bashdb") != NULL || strstr(basename, "shell") != NULL) {
        return MD_DEBUGGER_SHELL;
    }
    
    return MD_DEBUGGER_CUSTOM;
}

md_error_t md_adapter_find_debugger(md_debugger_type_t type,
                                     char *path,
                                     size_t path_size) {
    const char *debugger_names[] = {
        NULL,  /* MD_DEBUGGER_NONE */
        "lldb-dap", /* MD_DEBUGGER_LLDB */
        "gdb",  /* MD_DEBUGGER_GDB */
        "bashdb", /* MD_DEBUGGER_SHELL */
        NULL,  /* MD_DEBUGGER_CUSTOM */
    };
    
    const char *debugger_paths[] = {
        NULL,
        "/usr/bin/lldb-dap",
        "/usr/bin/gdb",
        "/usr/bin/bashdb",
        NULL,
    };
    
    if (type < 0 || type > MD_DEBUGGER_CUSTOM) {
        return MD_ERROR_INVALID_ARG;
    }
    
    /* Check known paths first */
    if (debugger_paths[type] != NULL) {
        struct stat st;
        if (stat(debugger_paths[type], &st) == 0 && S_ISREG(st.st_mode) &&
            (st.st_mode & S_IXUSR)) {
            strncpy(path, debugger_paths[type], path_size - 1);
            path[path_size - 1] = '\0';
            return MD_SUCCESS;
        }
    }
    
    /* Try to find in PATH */
    if (debugger_names[type] != NULL) {
        char *path_env = getenv("PATH");
        if (path_env != NULL) {
            char *path_copy = strdup(path_env);
            char *dir = strtok(path_copy, ":");
            
            while (dir != NULL) {
                snprintf(path, path_size, "%s/%s", dir, debugger_names[type]);
                struct stat st;
                if (stat(path, &st) == 0 && S_ISREG(st.st_mode) &&
                    (st.st_mode & S_IXUSR)) {
                    free(path_copy);
                    return MD_SUCCESS;
                }
                dir = strtok(NULL, ":");
            }
            
            free(path_copy);
        }
    }
    
    return MD_ERROR_NOT_FOUND;
}
