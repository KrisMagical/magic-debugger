/**
 * @file lldb_adapter.c
 * @brief Implementation of LLDB Debugger Adapter
 *
 * This file implements the LLDB adapter using the lldb-dap protocol.
 *
 * Phase 4: Adapter Layer - LLDB Integration
 */

#include "lldb_adapter.h"
#include "adapter.h"
#include "logger.h"
#include "session_manager.h"
#include "dap_client.h"
#include "state_model.h"
#include "dap_protocol.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ============================================================================
 * LLDB Adapter Internal Structure
 * ============================================================================ */

typedef struct lldb_adapter_data {
    lldb_adapter_options_t options;
    int current_thread_id;
    int current_frame_id;
    char target_path[MD_MAX_PATH_LEN];
} lldb_adapter_data_t;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static md_error_t lldb_init(md_adapter_t *adapter);
static void lldb_destroy(md_adapter_t *adapter);
static md_error_t lldb_launch(md_adapter_t *adapter);
static md_error_t lldb_attach(md_adapter_t *adapter, int pid);
static md_error_t lldb_disconnect(md_adapter_t *adapter, bool terminate_debuggee);
static md_error_t lldb_set_breakpoint(md_adapter_t *adapter, const char *path,
                                       int line, const char *condition, int *bp_id);
static md_error_t lldb_set_function_breakpoint(md_adapter_t *adapter,
                                                const char *function, int *bp_id);
static md_error_t lldb_remove_breakpoint(md_adapter_t *adapter, int bp_id);
static md_error_t lldb_enable_breakpoint(md_adapter_t *adapter, int bp_id, bool enable);
static md_error_t lldb_continue_exec(md_adapter_t *adapter);
static md_error_t lldb_pause(md_adapter_t *adapter);
static md_error_t lldb_step_over(md_adapter_t *adapter, md_step_granularity_t granularity);
static md_error_t lldb_step_into(md_adapter_t *adapter, md_step_granularity_t granularity);
static md_error_t lldb_step_out(md_adapter_t *adapter);
static md_error_t lldb_evaluate(md_adapter_t *adapter, const char *expression,
                                 int frame_id, md_eval_result_t *result);
static md_error_t lldb_set_variable(md_adapter_t *adapter, int var_ref,
                                     const char *name, const char *value);
static md_error_t lldb_get_threads(md_adapter_t *adapter, md_thread_t *threads,
                                    int max_count, int *actual_count);
static md_error_t lldb_get_stack_frames(md_adapter_t *adapter, int thread_id,
                                         md_stack_frame_t *frames,
                                         int max_count, int *actual_count);
static md_error_t lldb_get_scopes(md_adapter_t *adapter, int frame_id,
                                   md_scope_t *scopes, int max_count, int *actual_count);
static md_error_t lldb_get_variables(md_adapter_t *adapter, int var_ref,
                                      md_variable_t *variables,
                                      int max_count, int *actual_count);
static md_error_t lldb_set_active_thread(md_adapter_t *adapter, int thread_id);
static md_error_t lldb_set_active_frame(md_adapter_t *adapter, int frame_id);
static md_adapter_state_t lldb_get_state(md_adapter_t *adapter);
static md_error_t lldb_process_events(md_adapter_t *adapter, int timeout_ms);
static bool lldb_supports_feature(md_adapter_t *adapter, const char *feature);
static const char* lldb_get_name(md_adapter_t *adapter);
static const char* lldb_get_version(md_adapter_t *adapter);

/* ============================================================================
 * LLDB VTable
 * ============================================================================ */

static md_adapter_interface_t lldb_vtable = {
    .init = lldb_init,
    .destroy = lldb_destroy,
    .launch = lldb_launch,
    .attach = lldb_attach,
    .disconnect = lldb_disconnect,
    .set_breakpoint = lldb_set_breakpoint,
    .set_function_breakpoint = lldb_set_function_breakpoint,
    .remove_breakpoint = lldb_remove_breakpoint,
    .enable_breakpoint = lldb_enable_breakpoint,
    .continue_exec = lldb_continue_exec,
    .pause = lldb_pause,
    .step_over = lldb_step_over,
    .step_into = lldb_step_into,
    .step_out = lldb_step_out,
    .evaluate = lldb_evaluate,
    .set_variable = lldb_set_variable,
    .get_threads = lldb_get_threads,
    .get_stack_frames = lldb_get_stack_frames,
    .get_scopes = lldb_get_scopes,
    .get_variables = lldb_get_variables,
    .set_active_thread = lldb_set_active_thread,
    .set_active_frame = lldb_set_active_frame,
    .get_state = lldb_get_state,
    .process_events = lldb_process_events,
    .supports_feature = lldb_supports_feature,
    .get_name = lldb_get_name,
    .get_version = lldb_get_version,
};

/* ============================================================================
 * Event Handlers
 * ============================================================================ */

static void on_stopped_event(dap_event_t *event, void *user_data) {
    md_adapter_t *adapter = (md_adapter_t*)user_data;
    if (adapter == NULL || event == NULL) return;
    
    MD_DEBUG("LLDB stopped event received");
    adapter->state = MD_ADAPTER_STATE_STOPPED;
    
    if (adapter->state_model != NULL && event->body != NULL) {
        md_state_handle_stopped_event(adapter->state_model, (cJSON*)event->body);
    }
}

static void on_continued_event(dap_event_t *event, void *user_data) {
    md_adapter_t *adapter = (md_adapter_t*)user_data;
    if (adapter == NULL) return;
    
    MD_DEBUG("LLDB continued event received");
    adapter->state = MD_ADAPTER_STATE_RUNNING;
    
    if (adapter->state_model != NULL && event->body != NULL) {
        md_state_handle_continued_event(adapter->state_model, (cJSON*)event->body);
    }
}

static void on_exited_event(dap_event_t *event, void *user_data) {
    md_adapter_t *adapter = (md_adapter_t*)user_data;
    if (adapter == NULL) return;
    
    MD_DEBUG("LLDB exited event received");
    adapter->state = MD_ADAPTER_STATE_TERMINATED;
    
    if (adapter->state_model != NULL && event->body != NULL) {
        md_state_handle_exited_event(adapter->state_model, (cJSON*)event->body);
    }
}

static void on_terminated_event(dap_event_t *event, void *user_data) {
    md_adapter_t *adapter = (md_adapter_t*)user_data;
    if (adapter == NULL) return;
    
    MD_DEBUG("LLDB terminated event received");
    adapter->state = MD_ADAPTER_STATE_TERMINATED;
    
    if (adapter->state_model != NULL && event->body != NULL) {
        md_state_handle_terminated_event(adapter->state_model, (cJSON*)event->body);
    }
}

static void on_thread_event(dap_event_t *event, void *user_data) {
    md_adapter_t *adapter = (md_adapter_t*)user_data;
    if (adapter == NULL) return;
    
    MD_DEBUG("LLDB thread event received");
    
    if (adapter->state_model != NULL && event->body != NULL) {
        md_state_handle_thread_event(adapter->state_model, (cJSON*)event->body);
    }
}

static void on_breakpoint_event(dap_event_t *event, void *user_data) {
    md_adapter_t *adapter = (md_adapter_t*)user_data;
    if (adapter == NULL) return;
    
    MD_DEBUG("LLDB breakpoint event received");
    
    if (adapter->state_model != NULL && event->body != NULL) {
        md_state_handle_breakpoint_event(adapter->state_model, (cJSON*)event->body);
    }
}

static void on_output_event(dap_event_t *event, void *user_data) {
    md_adapter_t *adapter = (md_adapter_t*)user_data;
    if (adapter == NULL || event == NULL) return;
    
    /* Log output for debugging */
    cJSON *category = cJSON_GetObjectItemCaseSensitive((cJSON*)event->body, "category");
    cJSON *output = cJSON_GetObjectItemCaseSensitive((cJSON*)event->body, "output");
    
    const char *cat = category && cJSON_IsString(category) ? category->valuestring : "console";
    const char *out = output && cJSON_IsString(output) ? output->valuestring : "";
    
    MD_DEBUG("LLDB output [%s]: %s", cat, out);
}

/* ============================================================================
 * Lifecycle Operations
 * ============================================================================ */

static md_error_t lldb_init(md_adapter_t *adapter) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    MD_INFO("Initializing LLDB adapter");
    
    /* Initialize DAP client subsystem */
    md_error_t err = dap_client_init();
    if (err != MD_SUCCESS && err != MD_ERROR_ALREADY_INITIALIZED) {
        MD_ERROR("Failed to initialize DAP client subsystem");
        return err;
    }
    
    /* Create session configuration */
    md_session_config_t session_config;
    md_session_config_init(&session_config);
    
    /* Set debugger path */
    const char *debugger_path = adapter->config.debugger_path;
    if (debugger_path == NULL || debugger_path[0] == '\0') {
        char path[MD_MAX_PATH_LEN];
        err = md_adapter_find_debugger(MD_DEBUGGER_LLDB, path, sizeof(path));
        if (err != MD_SUCCESS) {
            MD_ERROR("lldb-dap not found");
            return MD_ERROR_NOT_FOUND;
        }
        debugger_path = path;
    }
    
    md_session_config_set_debugger(&session_config, debugger_path);
    
    /* Create session */
    adapter->session = md_session_create(NULL, &session_config);
    if (adapter->session == NULL) {
        MD_ERROR("Failed to create LLDB session");
        return MD_ERROR_OUT_OF_MEMORY;
    }
    
    /* Create DAP client */
    dap_client_config_t dap_config;
    dap_client_config_init(&dap_config);
    dap_config.timeout_ms = adapter->config.timeout_ms;
    dap_config.verbosity = adapter->config.verbosity;
    
    adapter->dap_client = dap_client_create(adapter->session, &dap_config);
    if (adapter->dap_client == NULL) {
        MD_ERROR("Failed to create DAP client");
        md_session_destroy(adapter->session);
        adapter->session = NULL;
        return MD_ERROR_OUT_OF_MEMORY;
    }
    
    /* Register event handlers */
    dap_client_register_event_handler(adapter->dap_client, DAP_EVENT_STOPPED, 
                                       on_stopped_event, adapter);
    dap_client_register_event_handler(adapter->dap_client, DAP_EVENT_CONTINUED,
                                       on_continued_event, adapter);
    dap_client_register_event_handler(adapter->dap_client, DAP_EVENT_EXITED,
                                       on_exited_event, adapter);
    dap_client_register_event_handler(adapter->dap_client, DAP_EVENT_TERMINATED,
                                       on_terminated_event, adapter);
    dap_client_register_event_handler(adapter->dap_client, DAP_EVENT_THREAD,
                                       on_thread_event, adapter);
    dap_client_register_event_handler(adapter->dap_client, DAP_EVENT_BREAKPOINT,
                                       on_breakpoint_event, adapter);
    dap_client_register_event_handler(adapter->dap_client, DAP_EVENT_OUTPUT,
                                       on_output_event, adapter);
    
    /* Create state model */
    md_state_config_t state_config;
    md_state_config_init(&state_config);
    adapter->state_model = md_state_model_create(&state_config);
    if (adapter->state_model == NULL) {
        MD_ERROR("Failed to create state model");
        dap_client_destroy(adapter->dap_client);
        md_session_destroy(adapter->session);
        adapter->dap_client = NULL;
        adapter->session = NULL;
        return MD_ERROR_OUT_OF_MEMORY;
    }
    
    /* Initialize DAP connection */
    dap_initialize_args_t init_args;
    memset(&init_args, 0, sizeof(init_args));
    init_args.client_id = "magic-debugger";
    init_args.client_name = "Magic Debugger";
    init_args.adapter_id = "lldb";
    init_args.lines_start_at1 = true;
    init_args.path_format = "path";
    init_args.supports_variable_type = true;
    init_args.supports_variable_paging = true;
    init_args.supports_run_in_terminal = false;
    
    err = dap_client_initialize(adapter->dap_client, &init_args, &adapter->capabilities);
    if (err != MD_SUCCESS) {
        MD_ERROR("Failed to initialize DAP connection: %d", err);
        return err;
    }
    
    adapter->capabilities_loaded = true;
    adapter->state = MD_ADAPTER_STATE_INITIALIZED;
    
    MD_INFO("LLDB adapter initialized successfully");
    return MD_SUCCESS;
}

static void lldb_destroy(md_adapter_t *adapter) {
    if (adapter == NULL) return;
    
    MD_DEBUG("Destroying LLDB adapter");
    
    if (adapter->user_data != NULL) {
        free(adapter->user_data);
        adapter->user_data = NULL;
    }
    
    adapter->state = MD_ADAPTER_STATE_NONE;
}

static md_error_t lldb_launch(md_adapter_t *adapter) {
    if (adapter == NULL || adapter->dap_client == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_INFO("Launching program with LLDB: %s", adapter->config.program_path);
    
    adapter->state = MD_ADAPTER_STATE_LAUNCHING;
    
    /* Build launch arguments */
    dap_launch_args_t launch_args;
    memset(&launch_args, 0, sizeof(launch_args));
    launch_args.program = adapter->config.program_path;
    launch_args.stop_on_entry = adapter->config.stop_on_entry;
    
    /* Create launch arguments JSON */
    cJSON *args = cJSON_CreateObject();
    if (args == NULL) return MD_ERROR_OUT_OF_MEMORY;
    
    cJSON_AddStringToObject(args, "program", adapter->config.program_path);
    cJSON_AddBoolToObject(args, "stopOnEntry", adapter->config.stop_on_entry);
    
    if (adapter->config.program_args[0] != '\0') {
        /* Parse args into array */
        cJSON *arg_array = cJSON_CreateArray();
        cJSON_AddItemToObject(args, "args", arg_array);
        
        char *args_copy = strdup(adapter->config.program_args);
        char *arg = strtok(args_copy, " ");
        while (arg != NULL) {
            cJSON_AddItemToArray(arg_array, cJSON_CreateString(arg));
            arg = strtok(NULL, " ");
        }
        free(args_copy);
    }
    
    if (adapter->config.working_dir[0] != '\0') {
        cJSON_AddStringToObject(args, "cwd", adapter->config.working_dir);
    }
    
    /* Store target path */
    lldb_adapter_data_t *data = (lldb_adapter_data_t*)adapter->user_data;
    if (data != NULL) {
        strncpy(data->target_path, adapter->config.program_path, MD_MAX_PATH_LEN - 1);
    }
    
    /* Send launch request */
    int seq = dap_client_get_seq(adapter->dap_client);
    md_error_t err = dap_client_send_request(adapter->dap_client, DAP_CMD_LAUNCH, args, seq);
    cJSON_Delete(args);
    
    if (err != MD_SUCCESS) {
        MD_ERROR("Failed to send launch request");
        return err;
    }
    
    /* Wait for response */
    err = dap_client_wait_for_response(adapter->dap_client, seq, adapter->config.timeout_ms);
    if (err != MD_SUCCESS) {
        MD_ERROR("Launch request failed or timed out");
        adapter->state = MD_ADAPTER_STATE_ERROR;
        return err;
    }
    
    /* Send configuration done */
    err = dap_client_configuration_done(adapter->dap_client);
    if (err != MD_SUCCESS) {
        MD_WARNING("Configuration done failed");
    }
    
    /* Update state model */
    if (adapter->state_model != NULL) {
        md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_LAUNCHING);
        strncpy(((md_debug_state_t*)adapter->state_model)->program_path,
                adapter->config.program_path, MD_MAX_PATH_LEN - 1);
    }
    
    MD_INFO("Program launched successfully");
    return MD_SUCCESS;
}

static md_error_t lldb_attach(md_adapter_t *adapter, int pid) {
    if (adapter == NULL || adapter->dap_client == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_INFO("Attaching to process %d with LLDB", pid);
    
    /* Build attach arguments */
    cJSON *args = cJSON_CreateObject();
    if (args == NULL) return MD_ERROR_OUT_OF_MEMORY;
    
    cJSON_AddNumberToObject(args, "processId", pid);
    
    /* Send attach request */
    int seq = dap_client_get_seq(adapter->dap_client);
    md_error_t err = dap_client_send_request(adapter->dap_client, "attach", args, seq);
    cJSON_Delete(args);
    
    if (err != MD_SUCCESS) return err;
    
    err = dap_client_wait_for_response(adapter->dap_client, seq, adapter->config.timeout_ms);
    if (err == MD_SUCCESS) {
        adapter->state = MD_ADAPTER_STATE_STOPPED;
        if (adapter->state_model != NULL) {
            md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_STOPPED);
        }
    }
    
    return err;
}

static md_error_t lldb_disconnect(md_adapter_t *adapter, bool terminate_debuggee) {
    if (adapter == NULL || adapter->dap_client == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_INFO("Disconnecting from LLDB");
    
    md_error_t err = dap_client_disconnect(adapter->dap_client, terminate_debuggee);
    
    adapter->state = MD_ADAPTER_STATE_TERMINATED;
    if (adapter->state_model != NULL) {
        md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_TERMINATED);
    }
    
    return err;
}

/* ============================================================================
 * Breakpoint Operations
 * ============================================================================ */

static md_error_t lldb_set_breakpoint(md_adapter_t *adapter, const char *path,
                                       int line, const char *condition, int *bp_id) {
    if (adapter == NULL || adapter->dap_client == NULL || path == NULL || line < 1) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_DEBUG("Setting breakpoint at %s:%d", path, line);
    
    /* Build breakpoints array */
    dap_source_breakpoint_t bp;
    memset(&bp, 0, sizeof(bp));
    bp.line = line;
    bp.condition = (char*)condition;
    
    dap_breakpoint_t *results = NULL;
    int result_count = 0;
    
    md_error_t err = dap_client_set_breakpoints(adapter->dap_client, path, &bp, 1,
                                                 &results, &result_count);
    
    if (err == MD_SUCCESS && results != NULL && result_count > 0) {
        if (bp_id != NULL) {
            *bp_id = results[0].id;
        }
        
        /* Update state model */
        if (adapter->state_model != NULL) {
            md_breakpoint_t mdbp;
            md_breakpoint_init(&mdbp);
            strncpy(mdbp.path, path, MD_MAX_PATH_LEN - 1);
            mdbp.line = line;
            mdbp.dap_id = results[0].id;
            mdbp.verified = results[0].verified;
            mdbp.state = results[0].verified ? MD_BP_STATE_VERIFIED : MD_BP_STATE_PENDING;
            if (results[0].message != NULL) {
                strncpy(mdbp.message, results[0].message, MD_MAX_ERROR_LEN - 1);
            }
            md_state_add_breakpoint(adapter->state_model, &mdbp);
        }
    }
    
    /* Cleanup results */
    if (results != NULL) {
        for (int i = 0; i < result_count; i++) {
            if (results[i].message != NULL) {
                free(results[i].message);
            }
        }
        free(results);
    }
    
    return err;
}

static md_error_t lldb_set_function_breakpoint(md_adapter_t *adapter,
                                                const char *function, int *bp_id) {
    (void)bp_id;  /* Will be set in response parsing */
    if (adapter == NULL || adapter->dap_client == NULL || function == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    if (!adapter->capabilities.supports_function_breakpoints) {
        MD_WARNING("Function breakpoints not supported");
        return MD_ERROR_NOT_SUPPORTED;
    }
    
    MD_DEBUG("Setting function breakpoint: %s", function);
    
    /* Build function breakpoint request */
    cJSON *args = cJSON_CreateObject();
    cJSON *filters = cJSON_CreateArray();
    cJSON *filter = cJSON_CreateObject();
    
    cJSON_AddStringToObject(filter, "name", function);
    cJSON_AddItemToArray(filters, filter);
    cJSON_AddItemToObject(args, "breakpoints", filters);
    
    int seq = dap_client_get_seq(adapter->dap_client);
    md_error_t err = dap_client_send_request(adapter->dap_client, 
                                              "setFunctionBreakpoints", args, seq);
    cJSON_Delete(args);
    
    if (err != MD_SUCCESS) return err;
    
    err = dap_client_wait_for_response(adapter->dap_client, seq, adapter->config.timeout_ms);
    
    return err;
}

static md_error_t lldb_remove_breakpoint(md_adapter_t *adapter, int bp_id) {
    if (adapter == NULL || adapter->dap_client == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_DEBUG("Removing breakpoint %d", bp_id);
    
    /* Find breakpoint in state model to get path */
    md_breakpoint_t bp;
    if (adapter->state_model != NULL) {
        if (md_state_find_breakpoint_by_dap_id(adapter->state_model, bp_id, &bp) != MD_SUCCESS) {
            return MD_ERROR_NOT_FOUND;
        }
    }
    
    /* Update state model */
    if (adapter->state_model != NULL) {
        md_state_remove_breakpoint(adapter->state_model, bp.id);
    }
    
    /* Note: DAP requires sending setBreakpoints with updated list to clear breakpoints */
    /* This is a simplified implementation */
    
    return MD_SUCCESS;
}

static md_error_t lldb_enable_breakpoint(md_adapter_t *adapter, int bp_id, bool enable) {
    if (adapter == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_DEBUG("%s breakpoint %d", enable ? "Enabling" : "Disabling", bp_id);
    
    if (adapter->state_model != NULL) {
        return md_state_set_breakpoint_enabled(adapter->state_model, bp_id, enable);
    }
    
    return MD_ERROR_NOT_SUPPORTED;
}

/* ============================================================================
 * Execution Control
 * ============================================================================ */

static md_error_t lldb_continue_exec(md_adapter_t *adapter) {
    if (adapter == NULL || adapter->dap_client == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_DEBUG("Continuing execution");
    
    int thread_id = md_adapter_get_current_thread_id(adapter);
    if (thread_id < 0) thread_id = 1;  /* Default to thread 1 */
    
    bool success = false;
    md_error_t err = dap_client_continue(adapter->dap_client, thread_id, &success);
    
    if (err == MD_SUCCESS && success) {
        adapter->state = MD_ADAPTER_STATE_RUNNING;
        if (adapter->state_model != NULL) {
            md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_RUNNING);
        }
    }
    
    return err;
}

static md_error_t lldb_pause(md_adapter_t *adapter) {
    if (adapter == NULL || adapter->dap_client == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_DEBUG("Pausing execution");
    
    /* Build pause request */
    cJSON *args = cJSON_CreateObject();
    int seq = dap_client_get_seq(adapter->dap_client);
    
    md_error_t err = dap_client_send_request(adapter->dap_client, "pause", args, seq);
    cJSON_Delete(args);
    
    if (err != MD_SUCCESS) return err;
    
    return dap_client_wait_for_response(adapter->dap_client, seq, adapter->config.timeout_ms);
}

static md_error_t lldb_step_over(md_adapter_t *adapter, md_step_granularity_t granularity) {
    if (adapter == NULL || adapter->dap_client == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_DEBUG("Stepping over (granularity: %s)", md_step_granularity_string(granularity));
    
    int thread_id = md_adapter_get_current_thread_id(adapter);
    if (thread_id < 0) thread_id = 1;
    
    md_error_t err = dap_client_next(adapter->dap_client, thread_id);
    
    if (err == MD_SUCCESS) {
        adapter->state = MD_ADAPTER_STATE_RUNNING;  /* Will transition to stopped after step */
        if (adapter->state_model != NULL) {
            md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_STEPPING);
        }
    }
    
    return err;
}

static md_error_t lldb_step_into(md_adapter_t *adapter, md_step_granularity_t granularity) {
    if (adapter == NULL || adapter->dap_client == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_DEBUG("Stepping into (granularity: %s)", md_step_granularity_string(granularity));
    
    int thread_id = md_adapter_get_current_thread_id(adapter);
    if (thread_id < 0) thread_id = 1;
    
    md_error_t err = dap_client_step_in(adapter->dap_client, thread_id);
    
    if (err == MD_SUCCESS) {
        adapter->state = MD_ADAPTER_STATE_RUNNING;
        if (adapter->state_model != NULL) {
            md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_STEPPING);
        }
    }
    
    return err;
}

static md_error_t lldb_step_out(md_adapter_t *adapter) {
    if (adapter == NULL || adapter->dap_client == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_DEBUG("Stepping out");
    
    int thread_id = md_adapter_get_current_thread_id(adapter);
    if (thread_id < 0) thread_id = 1;
    
    md_error_t err = dap_client_step_out(adapter->dap_client, thread_id);
    
    if (err == MD_SUCCESS) {
        adapter->state = MD_ADAPTER_STATE_RUNNING;
        if (adapter->state_model != NULL) {
            md_state_set_exec_state(adapter->state_model, MD_EXEC_STATE_STEPPING);
        }
    }
    
    return err;
}

/* ============================================================================
 * Variable Operations
 * ============================================================================ */

static md_error_t lldb_evaluate(md_adapter_t *adapter, const char *expression,
                                 int frame_id, md_eval_result_t *result) {
    if (adapter == NULL || adapter->dap_client == NULL || expression == NULL || result == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_DEBUG("Evaluating: %s", expression);
    
    memset(result, 0, sizeof(md_eval_result_t));
    
    /* Build evaluate request */
    cJSON *args = cJSON_CreateObject();
    cJSON_AddStringToObject(args, "expression", expression);
    cJSON_AddStringToObject(args, "context", "repl");
    if (frame_id > 0) {
        cJSON_AddNumberToObject(args, "frameId", frame_id);
    }
    
    int seq = dap_client_get_seq(adapter->dap_client);
    md_error_t err = dap_client_send_request(adapter->dap_client, "evaluate", args, seq);
    cJSON_Delete(args);
    
    if (err != MD_SUCCESS) return err;
    
    err = dap_client_wait_for_response(adapter->dap_client, seq, adapter->config.timeout_ms);
    if (err != MD_SUCCESS) return err;
    
    /* Parse response - simplified */
    result->success = true;
    strcpy(result->value, "(evaluation result)");
    
    return MD_SUCCESS;
}

static md_error_t lldb_set_variable(md_adapter_t *adapter, int var_ref,
                                     const char *name, const char *value) {
    if (adapter == NULL || adapter->dap_client == NULL || name == NULL || value == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    if (!adapter->capabilities.supports_set_variable) {
        MD_WARNING("setVariable not supported");
        return MD_ERROR_NOT_SUPPORTED;
    }
    
    MD_DEBUG("Setting variable %s = %s", name, value);
    
    /* Build setVariable request */
    cJSON *args = cJSON_CreateObject();
    cJSON_AddNumberToObject(args, "variablesReference", var_ref);
    cJSON_AddStringToObject(args, "name", name);
    cJSON_AddStringToObject(args, "value", value);
    
    int seq = dap_client_get_seq(adapter->dap_client);
    md_error_t err = dap_client_send_request(adapter->dap_client, "setVariable", args, seq);
    cJSON_Delete(args);
    
    if (err != MD_SUCCESS) return err;
    
    return dap_client_wait_for_response(adapter->dap_client, seq, adapter->config.timeout_ms);
}

static md_error_t lldb_get_variables(md_adapter_t *adapter, int var_ref,
                                      md_variable_t *variables,
                                      int max_count, int *actual_count) {
    (void)variables;  /* Output via state model */
    (void)max_count;  /* Used in state model update */
    if (adapter == NULL || adapter->dap_client == NULL || actual_count == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    *actual_count = 0;
    
    MD_DEBUG("Getting variables for ref %d", var_ref);
    
    /* Build variables request */
    cJSON *args = cJSON_CreateObject();
    cJSON_AddNumberToObject(args, "variablesReference", var_ref);
    
    int seq = dap_client_get_seq(adapter->dap_client);
    md_error_t err = dap_client_send_request(adapter->dap_client, "variables", args, seq);
    cJSON_Delete(args);
    
    if (err != MD_SUCCESS) return err;
    
    err = dap_client_wait_for_response(adapter->dap_client, seq, adapter->config.timeout_ms);
    if (err != MD_SUCCESS) return err;
    
    /* Update state model and return */
    if (adapter->state_model != NULL) {
        /* Parse from pending response */
        /* Simplified - would need access to response body */
    }
    
    return MD_SUCCESS;
}

static md_error_t lldb_get_scopes(md_adapter_t *adapter, int frame_id,
                                   md_scope_t *scopes, int max_count, int *actual_count) {
    (void)scopes;  /* Output via state model */
    (void)max_count;  /* Used in state model update */
    if (adapter == NULL || adapter->dap_client == NULL || actual_count == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    *actual_count = 0;
    
    MD_DEBUG("Getting scopes for frame %d", frame_id);
    
    /* Build scopes request */
    cJSON *args = cJSON_CreateObject();
    cJSON_AddNumberToObject(args, "frameId", frame_id);
    
    int seq = dap_client_get_seq(adapter->dap_client);
    md_error_t err = dap_client_send_request(adapter->dap_client, "scopes", args, seq);
    cJSON_Delete(args);
    
    if (err != MD_SUCCESS) return err;
    
    err = dap_client_wait_for_response(adapter->dap_client, seq, adapter->config.timeout_ms);
    
    return err;
}

/* ============================================================================
 * Stack and Thread Operations
 * ============================================================================ */

static md_error_t lldb_get_threads(md_adapter_t *adapter, md_thread_t *threads,
                                    int max_count, int *actual_count) {
    (void)threads;  /* Output via state model */
    (void)max_count;  /* Used in state model update */
    if (adapter == NULL || adapter->dap_client == NULL || actual_count == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    *actual_count = 0;
    
    MD_DEBUG("Getting threads");
    
    /* Build threads request */
    int seq = dap_client_get_seq(adapter->dap_client);
    md_error_t err = dap_client_send_request(adapter->dap_client, "threads", NULL, seq);
    
    if (err != MD_SUCCESS) return err;
    
    err = dap_client_wait_for_response(adapter->dap_client, seq, adapter->config.timeout_ms);
    if (err != MD_SUCCESS) return err;
    
    /* Would parse response here */
    
    return MD_SUCCESS;
}

static md_error_t lldb_get_stack_frames(md_adapter_t *adapter, int thread_id,
                                         md_stack_frame_t *frames,
                                         int max_count, int *actual_count) {
    if (adapter == NULL || adapter->dap_client == NULL || frames == NULL || actual_count == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    *actual_count = 0;
    
    if (thread_id < 0) {
        thread_id = md_adapter_get_current_thread_id(adapter);
        if (thread_id < 0) thread_id = 1;
    }
    
    MD_DEBUG("Getting stack frames for thread %d", thread_id);
    
    /* Build stackTrace request */
    cJSON *args = cJSON_CreateObject();
    cJSON_AddNumberToObject(args, "threadId", thread_id);
    cJSON_AddNumberToObject(args, "startFrame", 0);
    cJSON_AddNumberToObject(args, "levels", max_count);
    
    int seq = dap_client_get_seq(adapter->dap_client);
    md_error_t err = dap_client_send_request(adapter->dap_client, "stackTrace", args, seq);
    cJSON_Delete(args);
    
    if (err != MD_SUCCESS) return err;
    
    err = dap_client_wait_for_response(adapter->dap_client, seq, adapter->config.timeout_ms);
    
    return err;
}

static md_error_t lldb_set_active_thread(md_adapter_t *adapter, int thread_id) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    MD_DEBUG("Setting active thread: %d", thread_id);
    
    lldb_adapter_data_t *data = (lldb_adapter_data_t*)adapter->user_data;
    if (data != NULL) {
        data->current_thread_id = thread_id;
    }
    
    if (adapter->state_model != NULL) {
        return md_state_set_current_thread_id(adapter->state_model, thread_id);
    }
    
    return MD_SUCCESS;
}

static md_error_t lldb_set_active_frame(md_adapter_t *adapter, int frame_id) {
    if (adapter == NULL) return MD_ERROR_INVALID_ARG;
    
    MD_DEBUG("Setting active frame: %d", frame_id);
    
    lldb_adapter_data_t *data = (lldb_adapter_data_t*)adapter->user_data;
    if (data != NULL) {
        data->current_frame_id = frame_id;
    }
    
    if (adapter->state_model != NULL) {
        return md_state_set_current_frame_id(adapter->state_model, frame_id);
    }
    
    return MD_SUCCESS;
}

/* ============================================================================
 * State Query Operations
 * ============================================================================ */

static md_adapter_state_t lldb_get_state(md_adapter_t *adapter) {
    return adapter != NULL ? adapter->state : MD_ADAPTER_STATE_NONE;
}

static md_error_t lldb_process_events(md_adapter_t *adapter, int timeout_ms) {
    if (adapter == NULL || adapter->dap_client == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    return dap_client_process_messages(adapter->dap_client, timeout_ms);
}

static bool lldb_supports_feature(md_adapter_t *adapter, const char *feature) {
    if (adapter == NULL || feature == NULL || !adapter->capabilities_loaded) {
        return false;
    }
    
    if (strcmp(feature, "conditionalBreakpoints") == 0) {
        return adapter->capabilities.supports_conditional_breakpoints;
    }
    if (strcmp(feature, "functionBreakpoints") == 0) {
        return adapter->capabilities.supports_function_breakpoints;
    }
    if (strcmp(feature, "logPoints") == 0) {
        return adapter->capabilities.supports_log_points;
    }
    if (strcmp(feature, "setVariable") == 0) {
        return adapter->capabilities.supports_set_variable;
    }
    if (strcmp(feature, "stepBack") == 0) {
        return adapter->capabilities.supports_step_back;
    }
    
    return false;
}

static const char* lldb_get_name(md_adapter_t *adapter) {
    (void)adapter;
    return "LLDB";
}

static const char* lldb_get_version(md_adapter_t *adapter) {
    if (adapter == NULL) return "0.0";
    return adapter->version;
}

/* ============================================================================
 * LLDB-Specific Operations
 * ============================================================================ */

md_error_t md_lldb_execute_command(md_adapter_t *adapter,
                                    const char *command,
                                    char *output,
                                    size_t output_size) {
    if (adapter == NULL || command == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_DEBUG("Executing LLDB command: %s", command);
    
    /* Use evaluate with "repl" context for commands */
    md_eval_result_t result;
    md_error_t err = lldb_evaluate(adapter, command, MD_INVALID_ID, &result);
    
    if (err == MD_SUCCESS && output != NULL && output_size > 0) {
        strncpy(output, result.value, output_size - 1);
        output[output_size - 1] = '\0';
    }
    
    return err;
}

md_error_t md_lldb_get_version(md_adapter_t *adapter,
                                char *version,
                                size_t version_size) {
    if (adapter == NULL || version == NULL || version_size == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    /* Would execute "version" command */
    strncpy(version, "LLDB 14.0+", version_size - 1);
    version[version_size - 1] = '\0';
    
    return MD_SUCCESS;
}

md_error_t md_lldb_load_symbols(md_adapter_t *adapter,
                                 const char *module_path,
                                 const char *symbol_path) {
    if (adapter == NULL || module_path == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_DEBUG("Loading symbols for: %s", module_path);
    
    /* Build custom command */
    char cmd[MD_MAX_PATH_LEN * 2];
    if (symbol_path != NULL) {
        snprintf(cmd, sizeof(cmd), "target symbols add -s %s %s", symbol_path, module_path);
    } else {
        snprintf(cmd, sizeof(cmd), "target symbols add %s", module_path);
    }
    
    char output[MD_MAX_PATH_LEN];
    return md_lldb_execute_command(adapter, cmd, output, sizeof(output));
}

md_error_t md_lldb_set_disassembly_flavor(md_adapter_t *adapter,
                                           const char *flavor) {
    if (adapter == NULL || flavor == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    MD_DEBUG("Setting disassembly flavor: %s", flavor);
    
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "settings set target.x86-disassembly-flavor %s", flavor);
    
    char output[MD_MAX_PATH_LEN];
    return md_lldb_execute_command(adapter, cmd, output, sizeof(output));
}

/* ============================================================================
 * LLDB Adapter Factory
 * ============================================================================ */

md_adapter_t* md_lldb_adapter_create(const md_adapter_config_t *config) {
    md_adapter_t *adapter = (md_adapter_t*)calloc(1, sizeof(md_adapter_t));
    if (adapter == NULL) {
        return NULL;
    }
    
    /* Initialize base adapter */
    strcpy(adapter->name, LLDB_ADAPTER_NAME);
    strcpy(adapter->version, "1.0");
    adapter->type = MD_DEBUGGER_LLDB;
    adapter->state = MD_ADAPTER_STATE_NONE;
    adapter->vtable = &lldb_vtable;
    
    /* Copy configuration */
    if (config != NULL) {
        memcpy(&adapter->config, config, sizeof(md_adapter_config_t));
    } else {
        md_adapter_config_init(&adapter->config);
    }
    
    /* Allocate LLDB-specific data */
    lldb_adapter_data_t *data = (lldb_adapter_data_t*)calloc(1, sizeof(lldb_adapter_data_t));
    if (data == NULL) {
        free(adapter);
        return NULL;
    }
    
    lldb_adapter_options_init(&data->options);
    data->current_thread_id = MD_INVALID_ID;
    data->current_frame_id = MD_INVALID_ID;
    
    adapter->user_data = data;
    
    return adapter;
}

void lldb_adapter_options_init(lldb_adapter_options_t *options) {
    if (options == NULL) return;
    
    memset(options, 0, sizeof(lldb_adapter_options_t));
    options->display_runtime_support_values = false;
    options->command_completions_enabled = true;
    options->supports_granular_stepping = true;
}
