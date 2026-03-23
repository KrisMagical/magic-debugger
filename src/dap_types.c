/**
 * @file dap_types.c
 * @brief Implementation of DAP types and */

#include "dap_types.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * String Conversion Functions
 * ============================================================================ */

const char* dap_msg_type_string(dap_msg_type_t type) {
    switch (type) {
        case DAP_MSG_TYPE_REQUEST:  return "request";
        case DAP_MSG_TYPE_RESPONSE: return "response";
        case DAP_MSG_TYPE_EVENT:   return "event";
        default:               return "unknown";
    }
}

const char* dap_error_string(dap_error_t error) {
    switch (error) {
        case DAP_ERROR_NONE:              return "No error";
        case DAP_ERROR_PARSE_FAILED:      return "JSON parse failed";
        case DAP_ERROR_INVALID_MESSAGE:  return "Invalid message format";
        case DAP_ERROR_MISSING_FIELD:    return "Required field missing";
        case DAP_ERROR_TYPE_MISMATCH:    return "Field type mismatch";
        case DAP_ERROR_BUFFER_TOO_SMALL: return "Buffer too small";
        case DAP_ERROR_TIMEOUT:          return "Operation timeout";
        case DAP_ERROR_CONNECTION_CLOSED: return "Connection closed";
        case DAP_ERROR_REQUEST_FAILED:   return "Request failed";
        case DAP_ERROR_NOT_SUPPORTED:    return "Feature not supported";
        case DAP_ERROR_INVALID_SEQ:      return "Invalid sequence number";
        case DAP_ERROR_CANCELED:         return "Request canceled";
        default:                          return "Unknown error";
    }
}

const char* dap_stop_reason_string(dap_stop_reason_t reason) {
    switch (reason) {
        case DAP_STOP_REASON_STEP:            return "step";
        case DAP_STOP_REASON_BREAKPOINT:      return "breakpoint";
        case DAP_STOP_REASON_EXCEPTION:       return "exception";
        case DAP_STOP_REASON_PAUSE:           return "pause";
        case DAP_STOP_REASON_ENTRY:           return "entry";
        case DAP_STOP_REASON_GOTO:            return "goto";
        case DAP_STOP_REASON_FUNCTION:        return "function breakpoint";
        case DAP_STOP_REASON_DATA_BREAKPOINT: return "data breakpoint";
        case DAP_STOP_REASON_INSTRUCTION:     return "instruction breakpoint";
        default:                              return "unknown";
    }
}

dap_stop_reason_t dap_stop_reason_from_string(const char *str) {
    if (str == NULL) return DAP_STOP_REASON_STEP;
    
    if (strcmp(str, "step") == 0) return DAP_STOP_REASON_STEP;
    if (strcmp(str, "breakpoint") == 0) return DAP_STOP_REASON_BREAKPOINT;
    if (strcmp(str, "exception") == 0) return DAP_STOP_REASON_EXCEPTION;
    if (strcmp(str, "pause") == 0) return DAP_STOP_REASON_PAUSE;
    if (strcmp(str, "entry") == 0) return DAP_STOP_REASON_ENTRY;
    if (strcmp(str, "goto") == 0) return DAP_STOP_REASON_GOTO;
    if (strcmp(str, "function breakpoint") == 0) return DAP_STOP_REASON_FUNCTION;
    if (strcmp(str, "data breakpoint") == 0) return DAP_STOP_REASON_DATA_BREAKPOINT;
    if (strcmp(str, "instruction breakpoint") == 0) return DAP_STOP_REASON_INSTRUCTION;
    
    return DAP_STOP_REASON_STEP;
}

/* ============================================================================
 * Initialization Functions
 * ============================================================================ */

void dap_client_capabilities_init(dap_client_capabilities_t *caps) {
    if (caps == NULL) return;
    memset(caps, 0, sizeof(dap_client_capabilities_t));
    caps->locale = NULL;
    caps->lines_start_at1_or_0 = NULL;
    caps->path_format = NULL;
    caps->supports_variable_type = false;
    caps->supports_variable_paging = false;
    caps->supports_run_in_terminal = false;
    caps->supports_memory_references = false;
    caps->supports_progress_reporting = false;
    caps->supports_invalidated_event = false;
    caps->supports_memory_event = false;
}

void dap_capabilities_init(dap_capabilities_t *caps) {
    if (caps == NULL) return;
    memset(caps, 0, sizeof(dap_capabilities_t));
    /* All boolean fields default to false */
}

void dap_launch_args_init(dap_launch_args_t *args) {
    if (args == NULL) return;
    memset(args, 0, sizeof(dap_launch_args_t));
    args->no_debug = false;
    args->stop_on_entry = false;
}

/* ============================================================================
 * Free Functions
 * ============================================================================ */

void dap_source_free(dap_source_t *source) {
    if (source == NULL) return;
    MD_SAFE_FREE(source->name);
    MD_SAFE_FREE(source->path);
    MD_SAFE_FREE(source->presentation_hint);
    MD_SAFE_FREE(source->origin);
}

void dap_breakpoint_free(dap_breakpoint_t *bp) {
    if (bp == NULL) return;
    MD_SAFE_FREE(bp->message);
    if (bp->source != NULL) {
        dap_source_free(bp->source);
        MD_SAFE_FREE(bp->source);
    }
    MD_SAFE_FREE(bp->instruction_reference);
}

void dap_stack_frame_free(dap_stack_frame_t *frame) {
    if (frame == NULL) return;
    MD_SAFE_FREE(frame->name);
    if (frame->source != NULL) {
        dap_source_free(frame->source);
        MD_SAFE_FREE(frame->source);
    }
    MD_SAFE_FREE(frame->presentation_hint);
    MD_SAFE_FREE(frame->instruction_pointer_reference);
}

void dap_variable_free(dap_variable_t *var) {
    if (var == NULL) return;
    MD_SAFE_FREE(var->name);
    MD_SAFE_FREE(var->value);
    MD_SAFE_FREE(var->type);
    MD_SAFE_FREE(var->presentation_hint);
    MD_SAFE_FREE(var->evaluate_name);
}

void dap_scope_free(dap_scope_t *scope) {
    if (scope == NULL) return;
    MD_SAFE_FREE(scope->name);
    MD_SAFE_FREE(scope->presentation_hint);
    if (scope->source != NULL) {
        dap_source_free(scope->source);
        MD_SAFE_FREE(scope->source);
    }
}

void dap_message_free(dap_message_t *msg) {
    if (msg == NULL) return;
    
    switch (msg->type) {
        case DAP_MSG_TYPE_REQUEST:
            MD_SAFE_FREE(msg->u.request.command);
            if (msg->u.request.arguments != NULL) {
                cJSON_Delete((cJSON *)msg->u.request.arguments);
            }
            break;
        case DAP_MSG_TYPE_RESPONSE:
            MD_SAFE_FREE(msg->u.response.command);
            MD_SAFE_FREE(msg->u.response.message);
            if (msg->u.response.body != NULL) {
                cJSON_Delete((cJSON *)msg->u.response.body);
            }
            break;
        case DAP_MSG_TYPE_EVENT:
            MD_SAFE_FREE(msg->u.event.event);
            if (msg->u.event.body != NULL) {
                cJSON_Delete((cJSON *)msg->u.event.body);
            }
            break;
        default:
            break;
    }
    
    MD_SAFE_FREE(msg->raw_json);
    free(msg);
}

void dap_capabilities_free(dap_capabilities_t *caps) {
    if (caps == NULL) return;
    MD_SAFE_FREE(caps->exception_breakpoint_filters);
    MD_SAFE_FREE(caps->additional_module_columns);
    MD_SAFE_FREE(caps->supported_checksum_algorithms);
}
