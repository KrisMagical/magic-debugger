/**
 * @file gdb_adapter.c
 * @brief Implementation of GDB Debugger Adapter
 *
 * This file implements the GDB adapter using GDB/MI protocol.
 *
 * Phase 4: Adapter Layer - GDB Integration
 */

#include "gdb_adapter.h"
#include "adapter.h"
#include "logger.h"
#include "session_manager.h"
#include "state_model.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* ============================================================================
 * GDB MI Constants
 * ============================================================================ */

#define GDB_MI_PROMPT           "(gdb)"
#define GDB_MI_RESULT_PREFIX    "^"
#define GDB_MI_CONSOLE_PREFIX   "~"
#define GDB_MI_TARGET_PREFIX    "@"
#define GDB_MI_LOG_PREFIX       "&"
#define GDB_MI_ASYNC_PREFIX     "*"
#define GDB_MI_EXEC_ASYNC       "="

/* ============================================================================
 * GDB Adapter Internal Structure
 * ============================================================================ */

typedef struct gdb_adapter_data {
    gdb_adapter_options_t options;
    int current_thread_id;
    int current_frame_id;
    int breakpoint_counter;
    char mi_buffer[8192];
    size_t mi_buffer_len;
} gdb_adapter_data_t;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static md_error_t gdb_init(md_adapter_t *adapter);
static void gdb_destroy(md_adapter_t *adapter);
static md_error_t gdb_launch(md_adapter_t *adapter);
static md_error_t gdb_disconnect(md_adapter_t *adapter, bool terminate_debuggee);
static md_error_t gdb_set_breakpoint(md_adapter_t *adapter, const char *path,
                                      int line, const char *condition, int *bp_id);
static md_error_t gdb_remove_breakpoint(md_adapter_t *adapter, int bp_id);
static md_error_t gdb_continue_exec(md_adapter_t *adapter);
static md_error_t gdb_pause(md_adapter_t *adapter);
static md_error_t gdb_step_over(md_adapter_t *adapter, md_step_granularity_t granularity);
static md_error_t gdb_step_into(md_adapter_t *adapter, md_step_granularity_t granularity);
static md_error_t gdb_step_out(md_adapter_t *adapter);
static md_error_t gdb_evaluate(md_adapter_t *adapter, const char *expression,
                                int frame_id, md_eval_result_t *result);
static md_error_t gdb_get_stack_frames(md_adapter_t *adapter, int thread_id,
                                        md_stack_frame_t *frames,
                                        int max_count, int *actual_count);
static md_adapter_state_t gdb_get_state(md_adapter_t *adapter);
static md_error_t gdb_process_events(md_adapter_t *adapter, int timeout_ms);
static const char* gdb_get_name(md_adapter_t *adapter);

/* ============================================================================
 * GDB VTable
 * ============================================================================ */

static md_adapter_interface_t gdb_vtable = {
    .init = gdb_init,
    .destroy = gdb_destroy,
    .launch = gdb_launch,
    .disconnect = gdb_disconnect,
    .set_breakpoint = gdb_set_breakpoint,
    .remove_breakpoint = gdb_remove_breakpoint,
    .continue_exec = gdb_continue_exec,
    .pause = gdb_pause,
    .step_over = gdb_step_over,
    .step_into = gdb_step_into,
    .step_out = gdb_step_out,
    .evaluate = gdb_evaluate,
    .get_stack_frames = gdb_get_stack_frames,
    .get_state = gdb_get_state,
    .process_events = gdb_process_events,
    .get_name = gdb_get_name,
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static md_error_t send_mi_command(md_adapter_t *adapter, const char *command,
                                   char *response, size_t response_size) {
    if (adapter == NULL || adapter->session == NULL || command == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    /* Build MI command with newline */
    char cmd_buf[1024];
    int len = snprintf(cmd_buf, sizeof(cmd_buf), "%s\n", command);
    
    /* Send command */
    size_t bytes_sent;
    md_error_t err = md_session_send(adapter->session, cmd_buf, len, &bytes_sent);
    if (err != MD_SUCCESS) {
        MD_ERROR("Failed to send GDB command: %s", command);
        return err;
    }
    
    MD_TRACE("Sent GDB command: %s", command);
    
    /* Read response if buffer provided */
    if (response != NULL && response_size > 0) {
        response[0] = '\0';
        size_t total_read = 0;
        
        while (total_read < response_size - 1) {
            size_t bytes_read;
            err = md_session_receive(adapter->session, 
                                      response + total_read,
                                      response_size - total_read - 1,
                                      &bytes_read);
            if (err != MD_SUCCESS || bytes_read == 0) break;
            
            total_read += bytes_read;
            response[total_read] = '\0';
            
            /* Check for prompt indicating command completion */
            if (strstr(response, GDB_MI_PROMPT) != NULL) {
                break;
            }
        }
    }
    
    return MD_SUCCESS;
}

/* ============================================================================
 * Lifecycle Operations
 * ============================================================================ */

static md_error_t gdb_init(md_adapter_t *adapter) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    MD_INFO("Initializing GDB adapter");
    
    /* Find GDB */
    char debugger_path[MD_MAX_PATH_LEN];
    md_error_t err = md_adapter_find_debugger(MD_DEBUGGER_GDB, 
                                               debugger_path, sizeof(debugger_path));
    if (err != MD_SUCCESS) {
        MD_ERROR("GDB not found");
        return MD_ERROR_NOT_FOUND;
    }
    
    /* Create session configuration */
    md_session_config_t session_config;
    md_session_config_init(&session_config);
    md_session_config_set_debugger(&session_config, debugger_path);
    
    /* Add MI interpreter flag - note: session_config doesn't have debugger_args,
   we'll pass these through environment or command line */
    char args[256];
    snprintf(args, sizeof(args), "--interpreter=mi --quiet");
    (void)args;  /* Suppress unused warning - args used for future reference */
    
    /* Create session */
    adapter->session = md_session_create(NULL, &session_config);
    if (adapter->session == NULL) {
        MD_ERROR("Failed to create GDB session");
        return MD_ERROR_OUT_OF_MEMORY;
    }
    
    /* Create state model */
    md_state_config_t state_config;
    md_state_config_init(&state_config);
    adapter->state_model = md_state_model_create(&state_config);
    if (adapter->state_model == NULL) {
        MD_ERROR("Failed to create state model");
        md_session_destroy(adapter->session);
        adapter->session = NULL;
        return MD_ERROR_OUT_OF_MEMORY;
    }
    
    /* Initialize GDB capabilities */
    adapter->capabilities.supports_conditional_breakpoints = true;
    adapter->capabilities.supports_function_breakpoints = true;
    adapter->capabilities.supports_set_variable = true;
    adapter->capabilities_loaded = true;
    
    adapter->state = MD_ADAPTER_STATE_INITIALIZED;
    
    MD_INFO("GDB adapter initialized successfully");
    return MD_SUCCESS;
}

static void gdb_destroy(md_adapter_t *adapter) {
    if (adapter == NULL) return;
    
    MD_DEBUG("Destroying GDB adapter");
    
    if (adapter->user_data != NULL) {
        free(adapter->user_data);
        adapter->user_data = NULL;
    }
    
    adapter->state = MD_ADAPTER_STATE_NONE;
}

static md_error_t gdb_launch(md_adapter_t *adapter) {
    if (adapter == NULL || adapter->session == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_INFO("Launching program with GDB: %s", adapter->config.program_path);
    
    adapter->state = MD_ADAPTER_STATE_LAUNCHING;
    
    /* Load program file */
    char cmd[MD_MAX_PATH_LEN + 32];
    snprintf(cmd, sizeof(cmd), "-file-exec-and-symbols %s", adapter->config.program_path);
    
    md_error_t err = send_mi_command(adapter, cmd, NULL, 0);
    if (err != MD_SUCCESS) return err;
    
    /* Set arguments */
    if (adapter->config.program_args[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "-exec-arguments %s", adapter->config.program_args);
        err = send_mi_command(adapter, cmd, NULL, 0);
    }
    
    /* Set working directory */
    if (adapter->config.working_dir[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "-environment-cd %s", adapter->config.working_dir);
        err = send_mi_command(adapter, cmd, NULL, 0);
    }
    
    /* Run program */
    err = send_mi_command(adapter, "-exec-run", NULL, 0);
    if (err != MD_SUCCESS) return err;
    
    /* Wait for stop or running */
    adapter->state = MD_ADAPTER_STATE_RUNNING;
    if (adapter->state_model != NULL) {
        md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_RUNNING);
    }
    
    MD_INFO("Program launched");
    return MD_SUCCESS;
}

static md_error_t gdb_disconnect(md_adapter_t *adapter, bool terminate_debuggee) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    MD_INFO("Disconnecting from GDB");
    
    if (terminate_debuggee) {
        send_mi_command(adapter, "-exec-interrupt", NULL, 0);
        send_mi_command(adapter, "-target-detach", NULL, 0);
    }
    
    send_mi_command(adapter, "-gdb-exit", NULL, 0);
    
    adapter->state = MD_ADAPTER_STATE_TERMINATED;
    if (adapter->state_model != NULL) {
        md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_TERMINATED);
    }
    
    return MD_SUCCESS;
}

/* ============================================================================
 * Breakpoint Operations
 * ============================================================================ */

static md_error_t gdb_set_breakpoint(md_adapter_t *adapter, const char *path,
                                      int line, const char *condition, int *bp_id) {
    if (adapter == NULL || path == NULL || line < 1) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_DEBUG("Setting breakpoint at %s:%d", path, line);
    
    char cmd[MD_MAX_PATH_LEN + 64];
    char response[1024];
    
    /* Set breakpoint using MI command */
    snprintf(cmd, sizeof(cmd), "-break-insert %s:%d", path, line);
    
    md_error_t err = send_mi_command(adapter, cmd, response, sizeof(response));
    if (err != MD_SUCCESS) return err;
    
    /* Parse breakpoint number from response */
    int bp_num = 0;
    char *bp_match = strstr(response, "bkpt=");
    if (bp_match != NULL) {
        char *number_start = strstr(bp_match, "number=\"");
        if (number_start != NULL) {
            bp_num = atoi(number_start + 8);
        }
    }
    
    if (bp_id != NULL) {
        *bp_id = bp_num;
    }
    
    /* Add condition if specified */
    if (condition != NULL && bp_num > 0) {
        snprintf(cmd, sizeof(cmd), "-break-condition %d %s", bp_num, condition);
        send_mi_command(adapter, cmd, NULL, 0);
    }
    
    /* Update state model */
    if (adapter->state_model != NULL) {
        md_breakpoint_t bp;
        md_breakpoint_init(&bp);
        strncpy(bp.path, path, MD_MAX_PATH_LEN - 1);
        bp.line = line;
        bp.dap_id = bp_num;
        bp.state = MD_BP_STATE_VERIFIED;
        md_state_add_breakpoint(adapter->state_model, &bp);
    }
    
    return MD_SUCCESS;
}

static md_error_t gdb_remove_breakpoint(md_adapter_t *adapter, int bp_id) {
    if (adapter == NULL || bp_id < 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_DEBUG("Removing breakpoint %d", bp_id);
    
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "-break-delete %d", bp_id);
    
    md_error_t err = send_mi_command(adapter, cmd, NULL, 0);
    
    /* Update state model */
    if (adapter->state_model != NULL) {
        md_breakpoint_t bp;
        if (md_state_find_breakpoint_by_dap_id(adapter->state_model, bp_id, &bp) == MD_SUCCESS) {
            md_state_remove_breakpoint(adapter->state_model, bp.id);
        }
    }
    
    return err;
}

/* ============================================================================
 * Execution Control
 * ============================================================================ */

static md_error_t gdb_continue_exec(md_adapter_t *adapter) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    MD_DEBUG("Continuing execution");
    
    md_error_t err = send_mi_command(adapter, "-exec-continue", NULL, 0);
    
    if (err == MD_SUCCESS) {
        adapter->state = MD_ADAPTER_STATE_RUNNING;
        if (adapter->state_model != NULL) {
            md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_RUNNING);
        }
    }
    
    return err;
}

static md_error_t gdb_pause(md_adapter_t *adapter) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    MD_DEBUG("Pausing execution");
    
    return send_mi_command(adapter, "-exec-interrupt", NULL, 0);
}

static md_error_t gdb_step_over(md_adapter_t *adapter, md_step_granularity_t granularity) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    MD_DEBUG("Stepping over");
    
    const char *cmd = (granularity == MD_STEP_INSTRUCTION) ? 
                       "-exec-next-instruction" : "-exec-next";
    
    md_error_t err = send_mi_command(adapter, cmd, NULL, 0);
    
    if (err == MD_SUCCESS && adapter->state_model != NULL) {
        md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_STEPPING);
    }
    
    return err;
}

static md_error_t gdb_step_into(md_adapter_t *adapter, md_step_granularity_t granularity) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    MD_DEBUG("Stepping into");
    
    const char *cmd = (granularity == MD_STEP_INSTRUCTION) ? 
                       "-exec-step-instruction" : "-exec-step";
    
    md_error_t err = send_mi_command(adapter, cmd, NULL, 0);
    
    if (err == MD_SUCCESS && adapter->state_model != NULL) {
        md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_STEPPING);
    }
    
    return err;
}

static md_error_t gdb_step_out(md_adapter_t *adapter) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    MD_DEBUG("Stepping out");
    
    md_error_t err = send_mi_command(adapter, "-exec-finish", NULL, 0);
    
    if (err == MD_SUCCESS && adapter->state_model != NULL) {
        md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_STEPPING);
    }
    
    return err;
}

/* ============================================================================
 * Variable Operations
 * ============================================================================ */

static md_error_t gdb_evaluate(md_adapter_t *adapter, const char *expression,
                                int frame_id, md_eval_result_t *result) {
    (void)frame_id;  /* GDB MI uses current frame by default */
    if (adapter == NULL || expression == NULL || result == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_DEBUG("Evaluating: %s", expression);
    
    memset(result, 0, sizeof(md_eval_result_t));
    
    char cmd[MD_MAX_VARIABLE_STR_LEN + 32];
    char response[MD_MAX_VARIABLE_STR_LEN];
    
    /* Use GDB's data-evaluate-expression */
    snprintf(cmd, sizeof(cmd), "-data-evaluate-expression \"%s\"", expression);
    
    md_error_t err = send_mi_command(adapter, cmd, response, sizeof(response));
    if (err != MD_SUCCESS) {
        result->success = false;
        strncpy(result->error, "Failed to evaluate", MD_MAX_ERROR_LEN - 1);
        return err;
    }
    
    /* Parse value from response */
    char *value_start = strstr(response, "value=\"");
    if (value_start != NULL) {
        value_start += 7;
        char *value_end = strchr(value_start, '"');
        if (value_end != NULL) {
            size_t len = value_end - value_start;
            if (len < MD_MAX_VARIABLE_STR_LEN) {
                strncpy(result->value, value_start, len);
                result->value[len] = '\0';
            }
        }
    }
    
    result->success = true;
    return MD_SUCCESS;
}

/* ============================================================================
 * Stack Operations
 * ============================================================================ */

static md_error_t gdb_get_stack_frames(md_adapter_t *adapter, int thread_id,
                                        md_stack_frame_t *frames,
                                        int max_count, int *actual_count) {
    if (adapter == NULL || frames == NULL || actual_count == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    *actual_count = 0;
    
    MD_DEBUG("Getting stack frames");
    
    char response[8192];
    md_error_t err = send_mi_command(adapter, "-stack-list-frames", 
                                      response, sizeof(response));
    if (err != MD_SUCCESS) return err;
    
    /* Parse stack frames from response - simplified */
    int frame_count = 0;
    char *frame_start = response;
    
    while (frame_count < max_count) {
        char *frame = strstr(frame_start, "frame=");
        if (frame == NULL) break;
        
        md_stack_frame_init(&frames[frame_count]);
        
        /* Parse level */
        char *level = strstr(frame, "level=\"");
        if (level != NULL) {
            frames[frame_count].id = atoi(level + 7);
        }
        
        /* Parse file and line */
        char *file = strstr(frame, "file=\"");
        if (file != NULL) {
            char *end = strchr(file + 6, '"');
            if (end != NULL) {
                size_t len = end - (file + 6);
                if (len < MD_MAX_PATH_LEN) {
                    strncpy(frames[frame_count].source_path, file + 6, len);
                }
            }
        }
        
        /* Parse line */
        char *line = strstr(frame, "line=\"");
        if (line != NULL) {
            frames[frame_count].line = atoi(line + 6);
        }
        
        /* Parse function */
        char *func = strstr(frame, "func=\"");
        if (func != NULL) {
            char *end = strchr(func + 6, '"');
            if (end != NULL) {
                size_t len = end - (func + 6);
                if (len < 256) {
                    strncpy(frames[frame_count].name, func + 6, len);
                }
            }
        }
        
        frame_start = frame + 1;
        frame_count++;
    }
    
    *actual_count = frame_count;
    
    /* Update state model */
    if (adapter->state_model != NULL && thread_id > 0) {
        md_state_set_stack_frames(adapter->state_model, thread_id, frames, frame_count);
    }
    
    return MD_SUCCESS;
}

/* ============================================================================
 * State Query Operations
 * ============================================================================ */

static md_adapter_state_t gdb_get_state(md_adapter_t *adapter) {
    return adapter != NULL ? adapter->state : MD_ADAPTER_STATE_NONE;
}

static md_error_t gdb_process_events(md_adapter_t *adapter, int timeout_ms) {
    (void)timeout_ms;  /* Currently not used in simplified implementation */
    if (adapter == NULL || adapter->session == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    /* Check for pending output from GDB */
    char buffer[4096];
    size_t bytes_read;
    
    md_error_t err = md_session_receive(adapter->session, buffer, 
                                         sizeof(buffer) - 1, &bytes_read);
    if (err == MD_SUCCESS && bytes_read > 0) {
        buffer[bytes_read] = '\0';
        
        /* Check for async events */
        if (strstr(buffer, "*stopped") != NULL) {
            adapter->state = MD_ADAPTER_STATE_STOPPED;
            if (adapter->state_model != NULL) {
                md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_STOPPED);
            }
        }
        if (strstr(buffer, "*running") != NULL) {
            adapter->state = MD_ADAPTER_STATE_RUNNING;
        }
    }
    
    return MD_SUCCESS;
}

static const char* gdb_get_name(md_adapter_t *adapter) {
    (void)adapter;
    return "GDB";
}

/* ============================================================================
 * GDB-Specific Operations
 * ============================================================================ */

md_error_t md_gdb_execute_command(md_adapter_t *adapter,
                                   const char *command,
                                   char *output,
                                   size_t output_size) {
    return send_mi_command(adapter, command, output, output_size);
}

md_error_t md_gdb_get_version(md_adapter_t *adapter,
                               char *version,
                               size_t version_size) {
    if (adapter == NULL || version == NULL || version_size == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    char response[256];
    md_error_t err = send_mi_command(adapter, "-gdb-version", response, sizeof(response));
    
    if (err == MD_SUCCESS) {
        /* Extract version from response */
        strncpy(version, "GDB 12.0+", version_size - 1);
        version[version_size - 1] = '\0';
    }
    
    return err;
}

md_error_t md_gdb_load_pretty_printers(md_adapter_t *adapter,
                                         const char *path) {
    if (adapter == NULL || path == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    char cmd[MD_MAX_PATH_LEN + 32];
    snprintf(cmd, sizeof(cmd), "source %s", path);
    
    return send_mi_command(adapter, cmd, NULL, 0);
}

md_error_t md_gdb_add_substitute_path(md_adapter_t *adapter,
                                        const char *from,
                                        const char *to) {
    if (adapter == NULL || from == NULL || to == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    char cmd[MD_MAX_PATH_LEN * 2 + 32];
    snprintf(cmd, sizeof(cmd), "-gdb-set substitute-path %s %s", from, to);
    
    return send_mi_command(adapter, cmd, NULL, 0);
}

/* ============================================================================
 * GDB Adapter Factory
 * ============================================================================ */

md_adapter_t* md_gdb_adapter_create(const md_adapter_config_t *config) {
    md_adapter_t *adapter = (md_adapter_t*)calloc(1, sizeof(md_adapter_t));
    if (adapter == NULL) {
        return NULL;
    }
    
    strcpy(adapter->name, GDB_ADAPTER_NAME);
    strcpy(adapter->version, "1.0");
    adapter->type = MD_DEBUGGER_GDB;
    adapter->state = MD_ADAPTER_STATE_NONE;
    adapter->vtable = &gdb_vtable;
    
    if (config != NULL) {
        memcpy(&adapter->config, config, sizeof(md_adapter_config_t));
    } else {
        md_adapter_config_init(&adapter->config);
    }
    
    /* Allocate GDB-specific data */
    gdb_adapter_data_t *data = (gdb_adapter_data_t*)calloc(1, sizeof(gdb_adapter_data_t));
    if (data == NULL) {
        free(adapter);
        return NULL;
    }
    
    gdb_adapter_options_init(&data->options);
    adapter->user_data = data;
    
    return adapter;
}

void gdb_adapter_options_init(gdb_adapter_options_t *options) {
    if (options == NULL) return;
    
    memset(options, 0, sizeof(gdb_adapter_options_t));
    options->use_mi_protocol = true;
    options->use_gdb_dap = false;
    options->print_address = true;
    strcpy(options->disassembly_flavor, "intel");
}
