/**
 * @file shell_adapter.c
 * @brief Implementation of Shell Debugger Adapter
 *
 * This file implements the Shell adapter using bashdb.
 *
 * Phase 4: Adapter Layer - Shell Integration
 */

#include "shell_adapter.h"
#include "adapter.h"
#include "logger.h"
#include "session_manager.h"
#include "state_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SHELL_PROMPT           "bashdb<"
#define SHELL_STOPPED_PREFIX   "stopped in "
#define SHELL_BREAKPOINT_HIT   "Breakpoint"
#define SHELL_LINE_PREFIX      "line "

/* ============================================================================
 * Shell Adapter Internal Structure
 * ============================================================================ */

typedef struct shell_adapter_data {
    shell_adapter_options_t options;
    int current_line;
    char current_script[MD_MAX_PATH_LEN];
    int breakpoint_counter;
    bool is_stopped;
    char output_buffer[16384];
    size_t output_len;
} shell_adapter_data_t;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static md_error_t shell_init(md_adapter_t *adapter);
static void shell_destroy(md_adapter_t *adapter);
static md_error_t shell_launch(md_adapter_t *adapter);
static md_error_t shell_disconnect(md_adapter_t *adapter, bool terminate_debuggee);
static md_error_t shell_set_breakpoint(md_adapter_t *adapter, const char *path,
                                        int line, const char *condition, int *bp_id);
static md_error_t shell_remove_breakpoint(md_adapter_t *adapter, int bp_id);
static md_error_t shell_continue_exec(md_adapter_t *adapter);
static md_error_t shell_step_over(md_adapter_t *adapter, md_step_granularity_t granularity);
static md_error_t shell_step_into(md_adapter_t *adapter, md_step_granularity_t granularity);
static md_error_t shell_step_out(md_adapter_t *adapter);
static md_error_t shell_evaluate(md_adapter_t *adapter, const char *expression,
                                  int frame_id, md_eval_result_t *result);
static md_adapter_state_t shell_get_state(md_adapter_t *adapter);
static md_error_t shell_process_events(md_adapter_t *adapter, int timeout_ms);
static const char* shell_get_name(md_adapter_t *adapter);

/* ============================================================================
 * Shell VTable
 * ============================================================================ */

static md_adapter_interface_t shell_vtable = {
    .init = shell_init,
    .destroy = shell_destroy,
    .launch = shell_launch,
    .disconnect = shell_disconnect,
    .set_breakpoint = shell_set_breakpoint,
    .remove_breakpoint = shell_remove_breakpoint,
    .continue_exec = shell_continue_exec,
    .step_over = shell_step_over,
    .step_into = shell_step_into,
    .step_out = shell_step_out,
    .evaluate = shell_evaluate,
    .get_state = shell_get_state,
    .process_events = shell_process_events,
    .get_name = shell_get_name,
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static md_error_t send_shell_command(md_adapter_t *adapter, const char *command,
                                      char *response, size_t response_size) {
    if (adapter == NULL || adapter->session == NULL || command == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    /* Build command with newline */
    char cmd_buf[1024];
    int len = snprintf(cmd_buf, sizeof(cmd_buf), "%s\n", command);
    
    /* Send command */
    size_t bytes_sent;
    md_error_t err = md_session_send(adapter->session, cmd_buf, len, &bytes_sent);
    if (err != MD_SUCCESS) {
        MD_ERROR("Failed to send shell command: %s", command);
        return err;
    }
    
    MD_TRACE("Sent shell command: %s", command);
    
    /* Read response */
    if (response != NULL && response_size > 0) {
        response[0] = '\0';
        size_t total_read = 0;
        int attempts = 0;
        
        while (total_read < response_size - 1 && attempts < 100) {
            size_t bytes_read;
            err = md_session_receive(adapter->session, 
                                      response + total_read,
                                      response_size - total_read - 1,
                                      &bytes_read);
            if (err != MD_SUCCESS || bytes_read == 0) {
                attempts++;
                struct timespec ts = {0, 10000000};  /* 10ms */
                nanosleep(&ts, NULL);
                continue;
            }
            
            total_read += bytes_read;
            response[total_read] = '\0';
            
            /* Check for prompt */
            if (strstr(response, SHELL_PROMPT) != NULL) {
                break;
            }
        }
    }
    
    return MD_SUCCESS;
}

static void parse_position(shell_adapter_data_t *data, const char *response) {
    if (data == NULL || response == NULL) return;
    
    /* Parse line number */
    const char *line_str = strstr(response, "line ");
    if (line_str != NULL) {
        data->current_line = atoi(line_str + 5);
    }
    
    /* Parse script path */
    const char *file_start = strstr(response, "file ");
    if (file_start != NULL) {
        file_start += 5;
        const char *file_end = strchr(file_start, '\n');
        if (file_end == NULL) file_end = strchr(file_start, '\r');
        if (file_end != NULL) {
            size_t len = file_end - file_start;
            if (len < MD_MAX_PATH_LEN) {
                strncpy(data->current_script, file_start, len);
                data->current_script[len] = '\0';
            }
        }
    }
}

/* ============================================================================
 * Lifecycle Operations
 * ============================================================================ */

static md_error_t shell_init(md_adapter_t *adapter) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    MD_INFO("Initializing Shell adapter");
    
    /* Find bashdb */
    char debugger_path[MD_MAX_PATH_LEN];
    md_error_t err = md_adapter_find_debugger(MD_DEBUGGER_SHELL, 
                                               debugger_path, sizeof(debugger_path));
    if (err != MD_SUCCESS) {
        MD_ERROR("bashdb not found");
        return MD_ERROR_NOT_FOUND;
    }
    
    /* Create session configuration */
    md_session_config_t session_config;
    md_session_config_init(&session_config);
    md_session_config_set_debugger(&session_config, debugger_path);
    
    /* Create session */
    adapter->session = md_session_create(NULL, &session_config);
    if (adapter->session == NULL) {
        MD_ERROR("Failed to create Shell session");
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
    
    /* Initialize capabilities */
    adapter->capabilities.supports_conditional_breakpoints = true;
    adapter->capabilities_loaded = true;
    
    adapter->state = MD_ADAPTER_STATE_INITIALIZED;
    
    MD_INFO("Shell adapter initialized successfully");
    return MD_SUCCESS;
}

static void shell_destroy(md_adapter_t *adapter) {
    if (adapter == NULL) return;
    
    MD_DEBUG("Destroying Shell adapter");
    
    if (adapter->user_data != NULL) {
        free(adapter->user_data);
        adapter->user_data = NULL;
    }
    
    adapter->state = MD_ADAPTER_STATE_NONE;
}

static md_error_t shell_launch(md_adapter_t *adapter) {
    if (adapter == NULL || adapter->session == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_INFO("Launching script with Shell debugger: %s", adapter->config.program_path);
    
    adapter->state = MD_ADAPTER_STATE_LAUNCHING;
    
    /* For bashdb, we need to restart with proper arguments */
    /* First, disconnect existing session */
    if (adapter->session != NULL) {
        md_session_destroy(adapter->session);
        adapter->session = NULL;
    }
    
    /* Create new session with script argument */
    md_session_config_t session_config;
    md_session_config_init(&session_config);
    
    /* Find bashdb */
    char debugger_path[MD_MAX_PATH_LEN];
    md_error_t err = md_adapter_find_debugger(MD_DEBUGGER_SHELL, 
                                               debugger_path, sizeof(debugger_path));
    if (err != MD_SUCCESS) {
        return MD_ERROR_NOT_FOUND;
    }
    
    md_session_config_set_debugger(&session_config, debugger_path);
    
    /* Build arguments - note: session_config doesn't have debugger_args,
       we'll pass these through environment or command line */
    char args[MD_MAX_PATH_LEN * 2];
    snprintf(args, sizeof(args), "-- %s", adapter->config.program_path);
    (void)args;  /* Suppress unused warning - args used for future reference */
    
    /* Create session */
    adapter->session = md_session_create(NULL, &session_config);
    if (adapter->session == NULL) {
        return MD_ERROR_OUT_OF_MEMORY;
    }
    
    /* Read initial output */
    char response[8192];
    err = send_shell_command(adapter, "", response, sizeof(response));
    
    /* Set initial breakpoint */
    shell_adapter_data_t *data = (shell_adapter_data_t*)adapter->user_data;
    if (data != NULL) {
        strncpy(data->current_script, adapter->config.program_path, MD_MAX_PATH_LEN - 1);
    }
    
    adapter->state = MD_ADAPTER_STATE_STOPPED;
    if (adapter->state_model != NULL) {
        md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_STOPPED);
        md_state_set_stop_reason(adapter->state_model, 
                                  MD_STOP_REASON_ENTRY, "Entry point");
    }
    
    MD_INFO("Script launched");
    return MD_SUCCESS;
}

static md_error_t shell_disconnect(md_adapter_t *adapter, bool terminate_debuggee) {
    (void)terminate_debuggee;  /* bashdb terminates on quit */
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    MD_INFO("Disconnecting from Shell debugger");
    
    send_shell_command(adapter, "quit", NULL, 0);
    
    adapter->state = MD_ADAPTER_STATE_TERMINATED;
    if (adapter->state_model != NULL) {
        md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_TERMINATED);
    }
    
    return MD_SUCCESS;
}

/* ============================================================================
 * Breakpoint Operations
 * ============================================================================ */

static md_error_t shell_set_breakpoint(md_adapter_t *adapter, const char *path,
                                        int line, const char *condition, int *bp_id) {
    if (adapter == NULL || line < 1) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_DEBUG("Setting breakpoint at line %d", line);
    
    shell_adapter_data_t *data = (shell_adapter_data_t*)adapter->user_data;
    
    char cmd[256];
    char response[1024];
    
    /* bashdb uses "break <line>" command */
    snprintf(cmd, sizeof(cmd), "break %d", line);
    
    md_error_t err = send_shell_command(adapter, cmd, response, sizeof(response));
    if (err != MD_SUCCESS) return err;
    
    /* Parse breakpoint number */
    int bp_num = 0;
    char *bp_match = strstr(response, "Breakpoint");
    if (bp_match != NULL) {
        bp_num = atoi(bp_match + 10);
    }
    
    if (bp_num == 0 && data != NULL) {
        bp_num = ++data->breakpoint_counter;
    }
    
    if (bp_id != NULL) {
        *bp_id = bp_num;
    }
    
    /* Add condition if specified (bashdb supports this) */
    if (condition != NULL && bp_num > 0) {
        snprintf(cmd, sizeof(cmd), "condition %d %s", bp_num, condition);
        send_shell_command(adapter, cmd, NULL, 0);
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

static md_error_t shell_remove_breakpoint(md_adapter_t *adapter, int bp_id) {
    if (adapter == NULL || bp_id < 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_DEBUG("Removing breakpoint %d", bp_id);
    
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "delete %d", bp_id);
    
    md_error_t err = send_shell_command(adapter, cmd, NULL, 0);
    
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

static md_error_t shell_continue_exec(md_adapter_t *adapter) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    MD_DEBUG("Continuing execution");
    
    char response[8192];
    md_error_t err = send_shell_command(adapter, "continue", response, sizeof(response));
    
    if (err == MD_SUCCESS) {
        /* Check if stopped or finished */
        if (strstr(response, "stopped") != NULL || 
            strstr(response, "Breakpoint") != NULL) {
            adapter->state = MD_ADAPTER_STATE_STOPPED;
            if (adapter->state_model != NULL) {
                md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_STOPPED);
            }
            
            /* Parse position */
            shell_adapter_data_t *data = (shell_adapter_data_t*)adapter->user_data;
            if (data != NULL) {
                parse_position(data, response);
                data->is_stopped = true;
            }
        } else if (strstr(response, "terminated") != NULL ||
                   strstr(response, "script exited") != NULL) {
            adapter->state = MD_ADAPTER_STATE_TERMINATED;
            if (adapter->state_model != NULL) {
                md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_TERMINATED);
            }
        } else {
            adapter->state = MD_ADAPTER_STATE_RUNNING;
            if (adapter->state_model != NULL) {
                md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_RUNNING);
            }
        }
    }
    
    return err;
}

static md_error_t shell_step_over(md_adapter_t *adapter, md_step_granularity_t granularity) {
    (void)granularity;  /* bashdb doesn't support instruction stepping */
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    MD_DEBUG("Stepping over");
    
    char response[8192];
    md_error_t err = send_shell_command(adapter, "next", response, sizeof(response));
    
    if (err == MD_SUCCESS) {
        adapter->state = MD_ADAPTER_STATE_STOPPED;
        
        /* Parse position */
        shell_adapter_data_t *data = (shell_adapter_data_t*)adapter->user_data;
        if (data != NULL) {
            parse_position(data, response);
            data->is_stopped = true;
        }
        
        if (adapter->state_model != NULL) {
            md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_STOPPED);
            md_state_set_stop_reason(adapter->state_model, MD_STOP_REASON_STEP, NULL);
        }
    }
    
    return err;
}

static md_error_t shell_step_into(md_adapter_t *adapter, md_step_granularity_t granularity) {
    (void)granularity;  /* bashdb doesn't support instruction stepping */
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    MD_DEBUG("Stepping into");
    
    char response[8192];
    md_error_t err = send_shell_command(adapter, "step", response, sizeof(response));
    
    if (err == MD_SUCCESS) {
        adapter->state = MD_ADAPTER_STATE_STOPPED;
        
        shell_adapter_data_t *data = (shell_adapter_data_t*)adapter->user_data;
        if (data != NULL) {
            parse_position(data, response);
            data->is_stopped = true;
        }
        
        if (adapter->state_model != NULL) {
            md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_STOPPED);
        }
    }
    
    return err;
}

static md_error_t shell_step_out(md_adapter_t *adapter) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    MD_DEBUG("Stepping out (finish)");
    
    char response[8192];
    md_error_t err = send_shell_command(adapter, "finish", response, sizeof(response));
    
    if (err == MD_SUCCESS) {
        adapter->state = MD_ADAPTER_STATE_STOPPED;
        
        shell_adapter_data_t *data = (shell_adapter_data_t*)adapter->user_data;
        if (data != NULL) {
            parse_position(data, response);
            data->is_stopped = true;
        }
        
        if (adapter->state_model != NULL) {
            md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_STOPPED);
        }
    }
    
    return err;
}

/* ============================================================================
 * Variable Operations
 * ============================================================================ */

static md_error_t shell_evaluate(md_adapter_t *adapter, const char *expression,
                                  int frame_id, md_eval_result_t *result) {
    (void)frame_id;  /* bashdb uses current frame */
    if (adapter == NULL || expression == NULL || result == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_DEBUG("Evaluating: %s", expression);
    
    memset(result, 0, sizeof(md_eval_result_t));
    
    char cmd[MD_MAX_VARIABLE_STR_LEN + 16];
    char response[MD_MAX_VARIABLE_STR_LEN];
    
    /* bashdb uses "examine" or "print" command */
    snprintf(cmd, sizeof(cmd), "print $%s", expression);
    
    md_error_t err = send_shell_command(adapter, cmd, response, sizeof(response));
    if (err != MD_SUCCESS) {
        result->success = false;
        strncpy(result->error, "Failed to evaluate", MD_MAX_ERROR_LEN - 1);
        return err;
    }
    
    /* Parse value - simplified */
    char *equal = strchr(response, '=');
    if (equal != NULL) {
        strncpy(result->value, equal + 2, MD_MAX_VARIABLE_STR_LEN - 1);
        /* Remove trailing newline */
        size_t len = strlen(result->value);
        if (len > 0 && result->value[len - 1] == '\n') {
            result->value[len - 1] = '\0';
        }
    }
    
    result->success = true;
    return MD_SUCCESS;
}

/* ============================================================================
 * State Query Operations
 * ============================================================================ */

static md_adapter_state_t shell_get_state(md_adapter_t *adapter) {
    return adapter != NULL ? adapter->state : MD_ADAPTER_STATE_NONE;
}

static md_error_t shell_process_events(md_adapter_t *adapter, int timeout_ms) {
    (void)timeout_ms;  /* Currently not used in simplified implementation */
    if (adapter == NULL || adapter->session == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    /* Check for pending output */
    char buffer[4096];
    size_t bytes_read;
    
    md_error_t err = md_session_receive(adapter->session, buffer, 
                                         sizeof(buffer) - 1, &bytes_read);
    if (err == MD_SUCCESS && bytes_read > 0) {
        buffer[bytes_read] = '\0';
        
        /* Parse for events */
        if (strstr(buffer, "stopped") != NULL) {
            adapter->state = MD_ADAPTER_STATE_STOPPED;
        }
        if (strstr(buffer, "terminated") != NULL) {
            adapter->state = MD_ADAPTER_STATE_TERMINATED;
        }
    }
    
    return MD_SUCCESS;
}

static const char* shell_get_name(md_adapter_t *adapter) {
    (void)adapter;
    return "Shell";
}

/* ============================================================================
 * Shell-Specific Operations
 * ============================================================================ */

md_error_t md_shell_execute_command(md_adapter_t *adapter,
                                     const char *command,
                                     char *output,
                                     size_t output_size) {
    return send_shell_command(adapter, command, output, output_size);
}

int md_shell_get_current_line(md_adapter_t *adapter) {
    if (adapter == NULL) return -1;
    
    shell_adapter_data_t *data = (shell_adapter_data_t*)adapter->user_data;
    return data != NULL ? data->current_line : -1;
}

md_error_t md_shell_get_current_script(md_adapter_t *adapter,
                                        char *path,
                                        size_t path_size) {
    if (adapter == NULL || path == NULL || path_size == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    shell_adapter_data_t *data = (shell_adapter_data_t*)adapter->user_data;
    if (data == NULL) return MD_ERROR_INVALID_STATE;
    
    strncpy(path, data->current_script, path_size - 1);
    path[path_size - 1] = '\0';
    
    return MD_SUCCESS;
}

bool md_shell_is_at_breakpoint(md_adapter_t *adapter) {
    if (adapter == NULL) return false;
    
    shell_adapter_data_t *data = (shell_adapter_data_t*)adapter->user_data;
    return data != NULL ? data->is_stopped : false;
}

/* ============================================================================
 * Shell Adapter Factory
 * ============================================================================ */

md_adapter_t* md_shell_adapter_create(const md_adapter_config_t *config) {
    md_adapter_t *adapter = (md_adapter_t*)calloc(1, sizeof(md_adapter_t));
    if (adapter == NULL) {
        return NULL;
    }
    
    strcpy(adapter->name, SHELL_ADAPTER_NAME);
    strcpy(adapter->version, "1.0");
    adapter->type = MD_DEBUGGER_SHELL;
    adapter->state = MD_ADAPTER_STATE_NONE;
    adapter->vtable = &shell_vtable;
    
    if (config != NULL) {
        memcpy(&adapter->config, config, sizeof(md_adapter_config_t));
    } else {
        md_adapter_config_init(&adapter->config);
    }
    
    /* Allocate shell-specific data */
    shell_adapter_data_t *data = (shell_adapter_data_t*)calloc(1, sizeof(shell_adapter_data_t));
    if (data == NULL) {
        free(adapter);
        return NULL;
    }
    
    shell_adapter_options_init(&data->options);
    data->current_line = -1;
    adapter->user_data = data;
    
    return adapter;
}

void shell_adapter_options_init(shell_adapter_options_t *options) {
    if (options == NULL) return;
    
    memset(options, 0, sizeof(shell_adapter_options_t));
    options->debug_mode = true;
    options->line_numbers = true;
    strcpy(options->shell_path, "/bin/bash");
}
