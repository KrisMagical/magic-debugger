/**
 * @file dap_client.c
 * @brief Implementation of DAP Client
 *
 * This file implements the Debug Adapter Protocol client,
 * providing communication with debug adapters like LLDB-DAP and GDB.
 */

#include "dap_client.h"
#include "dap_types.h"
#include "io_handler.h"
#include "session_manager.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>

/* ============================================================================
 * Internal State
 * ============================================================================ */

static bool g_initialized = false;

static void* dap_cjson_malloc(size_t size) {
    return md_alloc(1, size);
}

static void dap_cjson_free(void *ptr) {
    md_free(ptr);
}

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void parse_messages_from_buffer(dap_client_t *client);
static void process_message(dap_client_t *client, cJSON *json);
static void handle_response(dap_client_t *client, cJSON *json);
static void handle_event(dap_client_t *client, cJSON *json);
static dap_pending_request_t* find_pending(dap_client_t *client, int seq);
static md_error_t add_pending(dap_client_t *client, const char *command, int seq);
static void parse_caps(cJSON *caps, dap_capabilities_t *out_caps);

/* ============================================================================
 * Subsystem Initialization
 * ============================================================================ */

md_error_t dap_client_init(void) {
    if (g_initialized) {
        return MD_ERROR_ALREADY_INITIALIZED;
    }
    
    cJSON_Hooks hooks = { dap_cjson_malloc, dap_cjson_free };
    cJSON_InitHooks(&hooks);
    
    g_initialized = true;
    MD_DEBUG("DAP client subsystem initialized");
    return MD_SUCCESS;
}

void dap_client_shutdown(void) {
    g_initialized = false;
    MD_DEBUG("DAP client subsystem shutdown");
}

bool dap_client_is_subsystem_initialized(void) {
    return g_initialized;
}

/* ============================================================================
 * Client Creation/Destruction
 * ============================================================================ */

dap_client_t* dap_client_create(md_session_t *session, const dap_client_config_t *config) {
    if (!g_initialized) {
        MD_ERROR("DAP client subsystem not initialized");
        return NULL;
    }
    
    if (session == NULL) {
        return NULL;
    }
    
    dap_client_t *client = (dap_client_t*)calloc(1, sizeof(dap_client_t));
    if (client == NULL) {
        return NULL;
    }
    
    client->session = session;
    client->debugger_type = md_session_get_debugger_type(session);
    client->seq = 1;
    client->initialized = false;
    client->timeout_ms = DAP_DEFAULT_TIMEOUT_MS;
    client->max_pending = DAP_MAX_PENDING;
    client->max_event_handlers = DAP_MAX_EVENT_HANDLERS;
    
    if (config != NULL) {
        if (config->timeout_ms > 0) client->timeout_ms = config->timeout_ms;
        if (config->max_pending > 0) client->max_pending = config->max_pending;
        if (config->max_event_handlers > 0) client->max_event_handlers = config->max_event_handlers;
        client->verbosity = config->verbosity;
    }
    
    client->read_buffer = md_io_buffer_create(DAP_MAX_MESSAGE_SIZE);
    client->pending_requests = (dap_pending_request_t*)calloc(
        client->max_pending, sizeof(dap_pending_request_t));
    client->event_handlers = (dap_event_handler_t*)calloc(
        client->max_event_handlers, sizeof(dap_event_handler_t));
    
    if (client->read_buffer == NULL || client->pending_requests == NULL || 
        client->event_handlers == NULL) {
        md_io_buffer_destroy(client->read_buffer);
        free(client->pending_requests);
        free(client->event_handlers);
        free(client);
        return NULL;
    }
    
    dap_capabilities_init(&client->capabilities);
    
    MD_INFO("DAP client created");
    return client;
}

void dap_client_destroy(dap_client_t *client) {
    if (client == NULL) return;
    
    /* Cancel pending requests */
    for (int i = 0; i < client->pending_count; i++) {
        MD_SAFE_FREE(client->pending_requests[i].command);
        MD_SAFE_FREE(client->pending_requests[i].error_message);
        if (client->pending_requests[i].response_body != NULL) {
            cJSON_Delete((cJSON*)client->pending_requests[i].response_body);
        }
    }
    
    /* Free event handlers */
    for (int i = 0; i < client->event_handler_count; i++) {
        MD_SAFE_FREE(client->event_handlers[i].event_name);
    }
    
    md_io_buffer_destroy(client->read_buffer);
    free(client->pending_requests);
    free(client->event_handlers);
    dap_capabilities_free(&client->capabilities);
    free(client);
    
    MD_INFO("DAP client destroyed");
}

void dap_client_config_init(dap_client_config_t *config) {
    if (config == NULL) return;
    memset(config, 0, sizeof(dap_client_config_t));
    config->timeout_ms = DAP_DEFAULT_TIMEOUT_MS;
    config->max_pending = DAP_MAX_PENDING;
    config->max_event_handlers = DAP_MAX_EVENT_HANDLERS;
}

/* ============================================================================
 * Message Sending
 * ============================================================================ */

md_error_t dap_client_send_raw(dap_client_t *client, const char *json_str, size_t json_len) {
    if (!g_initialized || client == NULL || json_str == NULL || json_len == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    /* Build message with header */
    char header[128];
    int header_len = snprintf(header, sizeof(header), 
                              "Content-Length: %zu\r\n\r\n", json_len);
    
    /* Send header */
    size_t sent;
    md_error_t err = md_session_send(client->session, header, header_len, &sent);
    if (err != MD_SUCCESS) return err;
    
    /* Send body */
    err = md_session_send(client->session, json_str, json_len, &sent);
    if (err != MD_SUCCESS) return err;
    
    client->requests_sent++;
    MD_TRACE("Sent DAP message: %zu bytes", json_len);
    
    return MD_SUCCESS;
}

md_error_t dap_client_send_request(dap_client_t *client, const char *command,
                                    const cJSON *args, int seq) {
    if (!g_initialized || client == NULL || command == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    /* Build request JSON */
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return MD_ERROR_OUT_OF_MEMORY;
    
    cJSON_AddNumberToObject(root, "seq", seq);
    cJSON_AddStringToObject(root, "type", "request");
    cJSON_AddStringToObject(root, "command", command);
    
    if (args != NULL) {
        cJSON_AddItemToObject(root, "arguments", cJSON_Duplicate(args, 1));
    }
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (json_str == NULL) return MD_ERROR_OUT_OF_MEMORY;
    
    size_t json_len = strlen(json_str);
    md_error_t err = dap_client_send_raw(client, json_str, json_len);
    
    free(json_str);
    return err;
}

md_error_t dap_client_send_event(dap_client_t *client, const char *event_name,
                                  const cJSON *body) {
    if (!g_initialized || client == NULL || event_name == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return MD_ERROR_OUT_OF_MEMORY;
    
    cJSON_AddNumberToObject(root, "seq", client->seq++);
    cJSON_AddStringToObject(root, "type", "event");
    cJSON_AddStringToObject(root, "event", event_name);
    
    if (body != NULL) {
        cJSON_AddItemToObject(root, "body", cJSON_Duplicate(body, 1));
    }
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (json_str == NULL) return MD_ERROR_OUT_OF_MEMORY;
    
    size_t json_len = strlen(json_str);
    md_error_t err = dap_client_send_raw(client, json_str, json_len);
    
    free(json_str);
    return err;
}

/* ============================================================================
 * Message Receiving
 * ============================================================================ */

md_error_t dap_client_receive(dap_client_t *client, cJSON **message, int timeout_ms) {
    if (!g_initialized || client == NULL || message == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    /* Wait for data */
    int result = md_wait_readable(md_session_get_stdout_fd(client->session), timeout_ms);
    if (result <= 0) {
        return (result == 0) ? MD_ERROR_TIMEOUT : MD_ERROR_IO_ERROR;
    }
    
    /* Read data */
    char buf[4096];
    size_t bytes_read;
    md_error_t err = md_session_receive(client->session, buf, sizeof(buf), &bytes_read);
    if (err != MD_SUCCESS) return err;
    
    if (bytes_read == 0) {
        return MD_ERROR_CONNECTION_CLOSED;
    }
    
    /* Add to buffer */
    md_io_buffer_write(client->read_buffer, buf, bytes_read);
    
    /* Parse messages */
    parse_messages_from_buffer(client);
    
    *message = NULL;  /* Messages are handled via callbacks */
    return MD_SUCCESS;
}

md_error_t dap_client_process_messages(dap_client_t *client, int timeout_ms) {
    if (!g_initialized || client == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    int stdout_fd = md_session_get_stdout_fd(client->session);
    int stderr_fd = md_session_get_stderr_fd(client->session);
    
    md_poll_event_t events[2];
    int event_count = 0;
    
    events[event_count].fd = stdout_fd;
    events[event_count].events = MD_IO_EVENT_READ | MD_IO_EVENT_HANGUP;
    event_count++;
    
    if (stderr_fd >= 0) {
        events[event_count].fd = stderr_fd;
        events[event_count].events = MD_IO_EVENT_READ | MD_IO_EVENT_HANGUP;
        event_count++;
    }
    
    int result = md_poll(events, event_count, timeout_ms);
    if (result < 0) return MD_ERROR_IO_ERROR;
    if (result == 0) return MD_SUCCESS;
    
    for (int i = 0; i < event_count; i++) {
        if (events[i].revents & MD_IO_EVENT_READ) {
            char buf[4096];
            size_t bytes_read;
            md_error_t err;
            
            if (events[i].fd == stdout_fd) {
                err = md_session_receive(client->session, buf, sizeof(buf), &bytes_read);
            } else {
                err = md_session_read_stderr(client->session, buf, sizeof(buf), &bytes_read);
            }
            
            if (err == MD_SUCCESS && bytes_read > 0) {
                md_io_buffer_write(client->read_buffer, buf, bytes_read);
                parse_messages_from_buffer(client);
            }
        }
        
        if (events[i].revents & MD_IO_EVENT_HANGUP) {
            return MD_ERROR_CONNECTION_CLOSED;
        }
    }
    
    return MD_SUCCESS;
}

static void parse_messages_from_buffer(dap_client_t *client) {
    while (md_io_buffer_size(client->read_buffer) > 0) {
        /* Find Content-Length header */
        size_t header_len;
        if (!md_io_buffer_has_line(client->read_buffer, &header_len)) {
            return;  /* Need more data */
        }
        
        /* Read header line */
        char header[256];
        ssize_t n = md_io_buffer_read_line(client->read_buffer, header, sizeof(header));
        if (n <= 0) return;
        
        /* Parse Content-Length */
        int content_length = 0;
        if (strncmp(header, "Content-Length:", 15) == 0) {
            content_length = atoi(header + 15);
        }
        
        if (content_length <= 0 || content_length > DAP_MAX_MESSAGE_SIZE) {
            MD_ERROR("Invalid Content-Length: %d", content_length);
            continue;
        }
        
        /* Skip empty line after header */
        char empty[4];
        md_io_buffer_read_line(client->read_buffer, empty, sizeof(empty));
        
        /* Check if we have enough data */
        if (md_io_buffer_size(client->read_buffer) < (size_t)content_length) {
            return;  /* Need more data */
        }
        
        /* Read JSON body */
        char *json_str = (char*)malloc(content_length + 1);
        if (json_str == NULL) continue;
        
        md_io_buffer_read(client->read_buffer, json_str, content_length);
        json_str[content_length] = '\0';
        
        /* Parse JSON */
        cJSON *json = cJSON_Parse(json_str);
        if (json != NULL) {
            process_message(client, json);
            cJSON_Delete(json);
        } else {
            MD_ERROR("Failed to parse JSON: %s", json_str);
        }
        
        free(json_str);
    }
}

static void process_message(dap_client_t *client, cJSON *json) {
    cJSON *type = cJSON_GetObjectItemCaseSensitive(json, "type");
    if (type == NULL || !cJSON_IsString(type)) {
        MD_ERROR("Message missing type field");
        return;
    }
    
    const char *type_str = type->valuestring;
    
    if (strcmp(type_str, "response") == 0) {
        client->responses_received++;
        handle_response(client, json);
    } else if (strcmp(type_str, "event") == 0) {
        client->events_received++;
        handle_event(client, json);
    } else {
        MD_WARNING("Unknown message type: %s", type_str);
    }
}

static void handle_response(dap_client_t *client, cJSON *json) {
    cJSON *request_seq = cJSON_GetObjectItemCaseSensitive(json, "request_seq");
    if (request_seq == NULL || !cJSON_IsNumber(request_seq)) {
        return;
    }
    
    int seq = request_seq->valueint;
    dap_pending_request_t *pending = find_pending(client, seq);
    if (pending == NULL) {
        MD_WARNING("Received response for unknown request: seq=%d", seq);
        return;
    }
    
    cJSON *success = cJSON_GetObjectItemCaseSensitive(json, "success");
    pending->success = (success != NULL && cJSON_IsTrue(success));
    
    cJSON *message = cJSON_GetObjectItemCaseSensitive(json, "message");
    if (message != NULL && cJSON_IsString(message)) {
        pending->error_message = md_strdup(message->valuestring);
    }
    
    cJSON *body = cJSON_GetObjectItemCaseSensitive(json, "body");
    if (body != NULL) {
        pending->response_body = cJSON_Duplicate(body, 1);
    }
    
    pending->completed = true;
    
    /* Special handling for initialize response */
    if (strcmp(pending->command, DAP_CMD_INITIALIZE) == 0 && pending->success) {
        parse_caps((cJSON*)pending->response_body, &client->capabilities);
        client->initialized = true;
    }
}

static void handle_event(dap_client_t *client, cJSON *json) {
    cJSON *event = cJSON_GetObjectItemCaseSensitive(json, "event");
    if (event == NULL || !cJSON_IsString(event)) {
        return;
    }
    
    const char *event_name = event->valuestring;
    MD_DEBUG("Received event: %s", event_name);
    
    cJSON *body = cJSON_GetObjectItemCaseSensitive(json, "body");
    
    /* Special handling for initialized event */
    if (strcmp(event_name, DAP_EVENT_INITIALIZED) == 0) {
        client->received_initialized_event = true;
        MD_INFO("Received initialized event from debugger");
    }
    
    /* Dispatch to registered handlers */
    for (int i = 0; i < client->event_handler_count; i++) {
        if (strcmp(client->event_handlers[i].event_name, event_name) == 0) {
            dap_event_t evt;
            memset(&evt, 0, sizeof(evt));
            evt.event = (char*)event_name;
            evt.body = body ? cJSON_Duplicate(body, 1) : NULL;
            
            client->event_handlers[i].callback(&evt, client->event_handlers[i].user_data);
            
            if (evt.body != NULL) {
                cJSON_Delete((cJSON*)evt.body);
            }
        }
    }
}

static dap_pending_request_t* find_pending(dap_client_t *client, int seq) {
    for (int i = 0; i < client->pending_count; i++) {
        if (client->pending_requests[i].seq == seq) {
            return &client->pending_requests[i];
        }
    }
    return NULL;
}

static md_error_t add_pending(dap_client_t *client, const char *command, int seq) {
    if (client->pending_count >= client->max_pending) {
        return MD_ERROR_BUFFER_OVERFLOW;
    }
    
    dap_pending_request_t *pending = &client->pending_requests[client->pending_count];
    memset(pending, 0, sizeof(dap_pending_request_t));
    pending->seq = seq;
    pending->command = md_strdup(command);
    pending->completed = false;
    pending->success = false;
    
    client->pending_count++;
    return MD_SUCCESS;
}

static void parse_caps(cJSON *caps, dap_capabilities_t *out_caps) {
    if (caps == NULL || out_caps == NULL) return;
    
    dap_capabilities_init(out_caps);
    
    cJSON *item;
#define PARSE_BOOL(name) \
    item = cJSON_GetObjectItemCaseSensitive(caps, #name); \
    if (item && cJSON_IsBool(item)) out_caps->name = cJSON_IsTrue(item)
    
    PARSE_BOOL(supports_configuration_done_request);
    PARSE_BOOL(supports_function_breakpoints);
    PARSE_BOOL(supports_conditional_breakpoints);
    PARSE_BOOL(supports_hit_conditional_breakpoints);
    PARSE_BOOL(supports_evaluate_for_hovers);
    PARSE_BOOL(supports_step_back);
    PARSE_BOOL(supports_set_variable);
    PARSE_BOOL(supports_restart_frame);
    PARSE_BOOL(supports_goto_targets_request);
    PARSE_BOOL(supports_step_in_targets_request);
    PARSE_BOOL(supports_completions_request);
    PARSE_BOOL(supports_modules_request);
    PARSE_BOOL(supports_exception_options);
    PARSE_BOOL(supports_value_formatting_options);
    PARSE_BOOL(supports_exception_info_request);
    PARSE_BOOL(support_terminate_debuggee);
    PARSE_BOOL(supports_delayed_stack_trace_loading);
    PARSE_BOOL(supports_loaded_sources_request);
    PARSE_BOOL(supports_log_points);
    PARSE_BOOL(supports_terminate_threads_request);
    PARSE_BOOL(supports_set_expression);
    PARSE_BOOL(supports_terminate_request);
    PARSE_BOOL(supports_data_breakpoints);
    PARSE_BOOL(supports_read_memory_request);
    PARSE_BOOL(supports_disassemble_request);
    PARSE_BOOL(supports_cancel_request);
    PARSE_BOOL(supports_breakpoint_locations_request);
    PARSE_BOOL(supports_clipboard_context);
    PARSE_BOOL(supports_stepping_granularity);
    PARSE_BOOL(supports_instruction_breakpoints);
    PARSE_BOOL(supports_filtering_by_source);
    PARSE_BOOL(supports_single_thread_execution_requests);
    
#undef PARSE_BOOL
}

/* ============================================================================
 * Event Handler Registration
 * ============================================================================ */

md_error_t dap_client_register_event_handler(dap_client_t *client, const char *event_name,
                                              dap_event_callback_t callback, void *user_data) {
    if (!g_initialized || client == NULL || event_name == NULL || callback == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    if (client->event_handler_count >= client->max_event_handlers) {
        return MD_ERROR_BUFFER_OVERFLOW;
    }
    
    client->event_handlers[client->event_handler_count].event_name = md_strdup(event_name);
    client->event_handlers[client->event_handler_count].callback = callback;
    client->event_handlers[client->event_handler_count].user_data = user_data;
    client->event_handler_count++;
    
    return MD_SUCCESS;
}

md_error_t dap_client_unregister_event_handler(dap_client_t *client, const char *event_name) {
    if (!g_initialized || client == NULL || event_name == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    for (int i = 0; i < client->event_handler_count; i++) {
        if (strcmp(client->event_handlers[i].event_name, event_name) == 0) {
            MD_SAFE_FREE(client->event_handlers[i].event_name);
            for (int j = i; j < client->event_handler_count - 1; j++) {
                client->event_handlers[j] = client->event_handlers[j + 1];
            }
            client->event_handler_count--;
            return MD_SUCCESS;
        }
    }
    
    return MD_ERROR_SESSION_NOT_FOUND;
}

bool dap_client_has_event_handler(dap_client_t *client, const char *event_name) {
    if (!g_initialized || client == NULL || event_name == NULL) {
        return false;
    }
    
    for (int i = 0; i < client->event_handler_count; i++) {
        if (strcmp(client->event_handlers[i].event_name, event_name) == 0) {
            return true;
        }
    }
    
    return false;
}

/* ============================================================================
 * Waiting for Response
 * ============================================================================ */

md_error_t dap_client_wait_for_response(dap_client_t *client, int seq, int timeout_ms) {
    if (!g_initialized || client == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    dap_pending_request_t *pending = find_pending(client, seq);
    if (pending == NULL) {
        return MD_ERROR_INVALID_SEQ;
    }
    
    int elapsed = 0;
    int poll_interval = 10;
    
    while (!pending->completed && elapsed < timeout_ms) {
        md_error_t err = dap_client_process_messages(client, poll_interval);
        if (err == MD_ERROR_CONNECTION_CLOSED) {
            return err;
        }
        elapsed += poll_interval;
    }
    
    if (!pending->completed) {
        return MD_ERROR_TIMEOUT;
    }
    
    if (!pending->success) {
        return MD_ERROR_REQUEST_FAILED;
    }
    
    return MD_SUCCESS;
}

/* ============================================================================
 * Core Commands
 * ============================================================================ */

md_error_t dap_client_initialize(dap_client_t *client, const dap_initialize_args_t *params,
                                 dap_capabilities_t *out_caps) {
    if (!g_initialized || client == NULL) {
        return MD_ERROR_NOT_INITIALIZED;
    }
    
    int seq = client->seq++;
    
    /* Build arguments */
    cJSON *args = cJSON_CreateObject();
    if (args == NULL) return MD_ERROR_OUT_OF_MEMORY;
    
    cJSON_AddStringToObject(args, "clientID", 
        params && params->client_id ? params->client_id : "magic-debugger");
    cJSON_AddStringToObject(args, "clientName",
        params && params->client_name ? params->client_name : "Magic Debugger");
    cJSON_AddStringToObject(args, "adapterID",
        params && params->adapter_id ? params->adapter_id : "default");
    cJSON_AddStringToObject(args, "locale",
        params && params->locale ? params->locale : "en-us");
    cJSON_AddStringToObject(args, "pathFormat",
        params && params->path_format ? params->path_format : "path");
    cJSON_AddBoolToObject(args, "linesStartAt1",
        params ? params->lines_start_at1 : true);
    
    /* Client capabilities */
    cJSON *caps = cJSON_CreateObject();
    cJSON_AddBoolToObject(caps, "supportsVariableType", 
        params ? params->supports_variable_type : true);
    cJSON_AddBoolToObject(caps, "supportsVariablePaging",
        params ? params->supports_variable_paging : true);
    cJSON_AddBoolToObject(caps, "supportsRunInTerminalRequest",
        params ? params->supports_run_in_terminal : true);
    cJSON_AddBoolToObject(caps, "supportsMemoryReferences",
        params ? params->supports_memory_references : false);
    cJSON_AddItemToObject(args, "supports", caps);
    
    md_error_t err = add_pending(client, DAP_CMD_INITIALIZE, seq);
    if (err != MD_SUCCESS) {
        cJSON_Delete(args);
        return err;
    }
    
    err = dap_client_send_request(client, DAP_CMD_INITIALIZE, args, seq);
    cJSON_Delete(args);
    
    if (err != MD_SUCCESS) {
        return err;
    }
    
    err = dap_client_wait_for_response(client, seq, client->timeout_ms);
    
    dap_pending_request_t *pending = find_pending(client, seq);
    if (pending != NULL && pending->success) {
        client->initialized = true;
        if (out_caps != NULL) {
            memcpy(out_caps, &client->capabilities, sizeof(dap_capabilities_t));
        }
    }
    
    return err;
}

md_error_t dap_client_launch(dap_client_t *client, const dap_launch_args_t *params,
                              bool *success) {
    if (!g_initialized || client == NULL) {
        return MD_ERROR_NOT_INITIALIZED;
    }
    
    int seq = client->seq++;
    
    cJSON *args = cJSON_CreateObject();
    if (args == NULL) return MD_ERROR_OUT_OF_MEMORY;
    
    if (params != NULL) {
        if (params->no_debug) {
            cJSON_AddBoolToObject(args, "noDebug", true);
        }
        if (params->stop_on_entry) {
            cJSON_AddBoolToObject(args, "stopOnEntry", true);
        }
        if (params->program != NULL) {
            cJSON_AddStringToObject(args, "program", params->program);
        }
        if (params->cwd != NULL) {
            cJSON_AddStringToObject(args, "cwd", params->cwd);
        }
    }
    
    md_error_t err = add_pending(client, DAP_CMD_LAUNCH, seq);
    if (err != MD_SUCCESS) {
        cJSON_Delete(args);
        return err;
    }
    
    err = dap_client_send_request(client, DAP_CMD_LAUNCH, args, seq);
    cJSON_Delete(args);
    
    if (err != MD_SUCCESS) {
        return err;
    }
    
    err = dap_client_wait_for_response(client, seq, client->timeout_ms);
    
    dap_pending_request_t *pending = find_pending(client, seq);
    if (pending != NULL && success != NULL) {
        *success = pending->success;
    }
    
    return err;
}

md_error_t dap_client_set_breakpoints(dap_client_t *client, const char *path,
                                       const dap_source_breakpoint_t *breakpoints,
                                       int count, dap_breakpoint_t **results,
                                       int *result_count) {
    if (!g_initialized || client == NULL || path == NULL || 
        breakpoints == NULL || count <= 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    int seq = client->seq++;
    
    cJSON *args = cJSON_CreateObject();
    if (args == NULL) return MD_ERROR_OUT_OF_MEMORY;
    
    cJSON *source = cJSON_CreateObject();
    cJSON_AddStringToObject(source, "path", path);
    cJSON_AddItemToObject(args, "source", source);
    
    cJSON *bps = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON *bp = cJSON_CreateObject();
        cJSON_AddNumberToObject(bp, "line", breakpoints[i].line);
        if (breakpoints[i].column > 0) {
            cJSON_AddNumberToObject(bp, "column", breakpoints[i].column);
        }
        if (breakpoints[i].condition != NULL) {
            cJSON_AddStringToObject(bp, "condition", breakpoints[i].condition);
        }
        if (breakpoints[i].hit_condition != NULL) {
            cJSON_AddStringToObject(bp, "hitCondition", breakpoints[i].hit_condition);
        }
        if (breakpoints[i].log_message != NULL) {
            cJSON_AddStringToObject(bp, "logMessage", breakpoints[i].log_message);
        }
        cJSON_AddItemToArray(bps, bp);
    }
    cJSON_AddItemToObject(args, "breakpoints", bps);
    
    md_error_t err = add_pending(client, DAP_CMD_SET_BREAKPOINTS, seq);
    if (err != MD_SUCCESS) {
        cJSON_Delete(args);
        return err;
    }
    
    err = dap_client_send_request(client, DAP_CMD_SET_BREAKPOINTS, args, seq);
    cJSON_Delete(args);
    
    if (err != MD_SUCCESS) {
        return err;
    }
    
    err = dap_client_wait_for_response(client, seq, client->timeout_ms);
    
    dap_pending_request_t *pending = find_pending(client, seq);
    if (pending != NULL && pending->success && pending->response_body != NULL) {
        cJSON *body = (cJSON*)pending->response_body;
        cJSON *bps_result = cJSON_GetObjectItemCaseSensitive(body, "breakpoints");
        
        if (bps_result != NULL && cJSON_IsArray(bps_result)) {
            int num_bps = cJSON_GetArraySize(bps_result);
            *result_count = num_bps < count ? num_bps : count;
            
            if (results != NULL && *result_count > 0) {
                *results = (dap_breakpoint_t*)calloc(*result_count, sizeof(dap_breakpoint_t));
                if (*results != NULL) {
                    for (int i = 0; i < *result_count; i++) {
                        cJSON *bp = cJSON_GetArrayItem(bps_result, i);
                        if (bp == NULL) continue;
                        
                        cJSON *id = cJSON_GetObjectItemCaseSensitive(bp, "id");
                        cJSON *verified = cJSON_GetObjectItemCaseSensitive(bp, "verified");
                        cJSON *line = cJSON_GetObjectItemCaseSensitive(bp, "line");
                        cJSON *msg = cJSON_GetObjectItemCaseSensitive(bp, "message");
                        
                        if (id && cJSON_IsNumber(id)) (*results)[i].id = id->valueint;
                        if (verified && cJSON_IsBool(verified)) (*results)[i].verified = cJSON_IsTrue(verified);
                        if (line && cJSON_IsNumber(line)) (*results)[i].line = line->valueint;
                        if (msg && cJSON_IsString(msg)) (*results)[i].message = md_strdup(msg->valuestring);
                    }
                }
            }
        }
    }
    
    return err;
}

md_error_t dap_client_configuration_done(dap_client_t *client) {
    if (!g_initialized || client == NULL) {
        return MD_ERROR_NOT_INITIALIZED;
    }
    
    int seq = client->seq++;
    
    md_error_t err = add_pending(client, DAP_CMD_CONFIGURATION_DONE, seq);
    if (err != MD_SUCCESS) return err;
    
    err = dap_client_send_request(client, DAP_CMD_CONFIGURATION_DONE, NULL, seq);
    if (err != MD_SUCCESS) return err;
    
    return dap_client_wait_for_response(client, seq, client->timeout_ms);
}

md_error_t dap_client_continue(dap_client_t *client, int thread_id, bool *success) {
    if (!g_initialized || client == NULL) {
        return MD_ERROR_NOT_INITIALIZED;
    }
    
    int seq = client->seq++;
    
    cJSON *args = cJSON_CreateObject();
    if (args == NULL) return MD_ERROR_OUT_OF_MEMORY;
    cJSON_AddNumberToObject(args, "threadId", thread_id);
    
    md_error_t err = add_pending(client, DAP_CMD_CONTINUE, seq);
    if (err != MD_SUCCESS) {
        cJSON_Delete(args);
        return err;
    }
    
    err = dap_client_send_request(client, DAP_CMD_CONTINUE, args, seq);
    cJSON_Delete(args);
    
    if (err != MD_SUCCESS) return err;
    
    err = dap_client_wait_for_response(client, seq, client->timeout_ms);
    
    dap_pending_request_t *pending = find_pending(client, seq);
    if (pending != NULL && success != NULL) {
        *success = pending->success;
    }
    
    return err;
}

md_error_t dap_client_next(dap_client_t *client, int thread_id) {
    if (!g_initialized || client == NULL) {
        return MD_ERROR_NOT_INITIALIZED;
    }
    
    int seq = client->seq++;
    
    cJSON *args = cJSON_CreateObject();
    if (args == NULL) return MD_ERROR_OUT_OF_MEMORY;
    cJSON_AddNumberToObject(args, "threadId", thread_id);
    
    md_error_t err = add_pending(client, DAP_CMD_NEXT, seq);
    if (err != MD_SUCCESS) {
        cJSON_Delete(args);
        return err;
    }
    
    err = dap_client_send_request(client, DAP_CMD_NEXT, args, seq);
    cJSON_Delete(args);
    
    if (err != MD_SUCCESS) return err;
    
    return dap_client_wait_for_response(client, seq, client->timeout_ms);
}

md_error_t dap_client_step_in(dap_client_t *client, int thread_id) {
    if (!g_initialized || client == NULL) {
        return MD_ERROR_NOT_INITIALIZED;
    }
    
    int seq = client->seq++;
    
    cJSON *args = cJSON_CreateObject();
    if (args == NULL) return MD_ERROR_OUT_OF_MEMORY;
    cJSON_AddNumberToObject(args, "threadId", thread_id);
    
    md_error_t err = add_pending(client, DAP_CMD_STEP_IN, seq);
    if (err != MD_SUCCESS) {
        cJSON_Delete(args);
        return err;
    }
    
    err = dap_client_send_request(client, DAP_CMD_STEP_IN, args, seq);
    cJSON_Delete(args);
    
    if (err != MD_SUCCESS) return err;
    
    return dap_client_wait_for_response(client, seq, client->timeout_ms);
}

md_error_t dap_client_step_out(dap_client_t *client, int thread_id) {
    if (!g_initialized || client == NULL) {
        return MD_ERROR_NOT_INITIALIZED;
    }
    
    int seq = client->seq++;
    
    cJSON *args = cJSON_CreateObject();
    if (args == NULL) return MD_ERROR_OUT_OF_MEMORY;
    cJSON_AddNumberToObject(args, "threadId", thread_id);
    
    md_error_t err = add_pending(client, DAP_CMD_STEP_OUT, seq);
    if (err != MD_SUCCESS) {
        cJSON_Delete(args);
        return err;
    }
    
    err = dap_client_send_request(client, DAP_CMD_STEP_OUT, args, seq);
    cJSON_Delete(args);
    
    if (err != MD_SUCCESS) return err;
    
    return dap_client_wait_for_response(client, seq, client->timeout_ms);
}

md_error_t dap_client_disconnect(dap_client_t *client, bool terminate_debuggee) {
    if (!g_initialized || client == NULL) {
        return MD_ERROR_NOT_INITIALIZED;
    }
    
    int seq = client->seq++;
    
    cJSON *args = cJSON_CreateObject();
    if (args == NULL) return MD_ERROR_OUT_OF_MEMORY;
    cJSON_AddBoolToObject(args, "terminateDebuggee", terminate_debuggee);
    
    md_error_t err = add_pending(client, DAP_CMD_DISCONNECT, seq);
    if (err != MD_SUCCESS) {
        cJSON_Delete(args);
        return err;
    }
    
    err = dap_client_send_request(client, DAP_CMD_DISCONNECT, args, seq);
    cJSON_Delete(args);
    
    if (err != MD_SUCCESS) return err;
    
    err = dap_client_wait_for_response(client, seq, client->timeout_ms);
    
    client->initialized = false;
    return err;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

bool dap_client_is_initialized(dap_client_t *client) {
    return client != NULL && client->initialized;
}

int dap_client_get_seq(dap_client_t *client) {
    return client != NULL ? client->seq : 00;
}

md_session_t* dap_client_get_session(dap_client_t *client) {
    return client != NULL ? client->session : NULL;
}

const dap_capabilities_t* dap_client_get_capabilities(dap_client_t *client) {
    return client != NULL ? &client->capabilities : NULL;
}

md_error_t dap_client_set_timeout(dap_client_t *client, int timeout_ms) {
    if (client == NULL) return MD_ERROR_INVALID_ARG;
    if (timeout_ms < 0 && timeout_ms != -1) return MD_ERROR_INVALID_ARG;
    client->timeout_ms = timeout_ms;
    return MD_SUCCESS;
}

md_error_t dap_client_get_timeout(dap_client_t *client, int *timeout_ms) {
    if (client == NULL || timeout_ms == NULL) return MD_ERROR_INVALID_ARG;
    *timeout_ms = client->timeout_ms;
    return MD_SUCCESS;
}
