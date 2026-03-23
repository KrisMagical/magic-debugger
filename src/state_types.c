/**
 * @file state_types.c
 * @brief Implementation of state type helper functions
 */

#include "state_types.h"
#include "dap_types.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Execution State Functions
 * ============================================================================ */

const char* md_exec_state_string(md_exec_state_t state) {
    switch (state) {
        case MD_EXEC_STATE_NONE:       return "none";
        case MD_EXEC_STATE_LAUNCHING:  return "launching";
        case MD_EXEC_STATE_RUNNING:    return "running";
        case MD_EXEC_STATE_STOPPED:    return "stopped";
        case MD_EXEC_STATE_STEPPING:   return "stepping";
        case MD_EXEC_STATE_EXITING:    return "exiting";
        case MD_EXEC_STATE_TERMINATED: return "terminated";
        case MD_EXEC_STATE_CRASHED:    return "crashed";
        default:                       return "unknown";
    }
}

bool md_exec_state_is_stopped(md_exec_state_t state) {
    return state == MD_EXEC_STATE_STOPPED || 
           state == MD_EXEC_STATE_CRASHED;
}

bool md_exec_state_is_running(md_exec_state_t state) {
    return state == MD_EXEC_STATE_RUNNING || 
           state == MD_EXEC_STATE_STEPPING ||
           state == MD_EXEC_STATE_LAUNCHING;
}

bool md_exec_state_is_terminated(md_exec_state_t state) {
    return state == MD_EXEC_STATE_TERMINATED || 
           state == MD_EXEC_STATE_EXITING ||
           state == MD_EXEC_STATE_CRASHED;
}

/* ============================================================================
 * Stop Reason Functions
 * ============================================================================ */

const char* md_stop_reason_string(md_stop_reason_t reason) {
    switch (reason) {
        case MD_STOP_REASON_UNKNOWN:             return "unknown";
        case MD_STOP_REASON_STEP:                return "step";
        case MD_STOP_REASON_BREAKPOINT:          return "breakpoint";
        case MD_STOP_REASON_EXCEPTION:           return "exception";
        case MD_STOP_REASON_PAUSE:               return "pause";
        case MD_STOP_REASON_ENTRY:               return "entry";
        case MD_STOP_REASON_GOTO:                return "goto";
        case MD_STOP_REASON_FUNCTION_BREAKPOINT: return "function breakpoint";
        case MD_STOP_REASON_DATA_BREAKPOINT:     return "data breakpoint";
        case MD_STOP_REASON_INSTRUCTION_BREAKPOINT: return "instruction breakpoint";
        case MD_STOP_REASON_SIGNAL:              return "signal";
        case MD_STOP_REASON_WATCHPOINT:          return "watchpoint";
        default:                                 return "unknown";
    }
}

md_stop_reason_t md_stop_reason_from_dap(dap_stop_reason_t dap_reason) {
    switch (dap_reason) {
        case DAP_STOP_REASON_STEP:            return MD_STOP_REASON_STEP;
        case DAP_STOP_REASON_BREAKPOINT:      return MD_STOP_REASON_BREAKPOINT;
        case DAP_STOP_REASON_EXCEPTION:       return MD_STOP_REASON_EXCEPTION;
        case DAP_STOP_REASON_PAUSE:           return MD_STOP_REASON_PAUSE;
        case DAP_STOP_REASON_ENTRY:           return MD_STOP_REASON_ENTRY;
        case DAP_STOP_REASON_GOTO:            return MD_STOP_REASON_GOTO;
        case DAP_STOP_REASON_FUNCTION:        return MD_STOP_REASON_FUNCTION_BREAKPOINT;
        case DAP_STOP_REASON_DATA_BREAKPOINT: return MD_STOP_REASON_DATA_BREAKPOINT;
        case DAP_STOP_REASON_INSTRUCTION:     return MD_STOP_REASON_INSTRUCTION_BREAKPOINT;
        default:                              return MD_STOP_REASON_UNKNOWN;
    }
}

md_stop_reason_t md_stop_reason_from_string(const char *str) {
    if (str == NULL) return MD_STOP_REASON_UNKNOWN;
    
    if (strcmp(str, "step") == 0) return MD_STOP_REASON_STEP;
    if (strcmp(str, "breakpoint") == 0) return MD_STOP_REASON_BREAKPOINT;
    if (strcmp(str, "exception") == 0) return MD_STOP_REASON_EXCEPTION;
    if (strcmp(str, "pause") == 0) return MD_STOP_REASON_PAUSE;
    if (strcmp(str, "entry") == 0) return MD_STOP_REASON_ENTRY;
    if (strcmp(str, "goto") == 0) return MD_STOP_REASON_GOTO;
    if (strcmp(str, "function breakpoint") == 0) return MD_STOP_REASON_FUNCTION_BREAKPOINT;
    if (strcmp(str, "data breakpoint") == 0) return MD_STOP_REASON_DATA_BREAKPOINT;
    if (strcmp(str, "instruction breakpoint") == 0) return MD_STOP_REASON_INSTRUCTION_BREAKPOINT;
    if (strcmp(str, "signal") == 0) return MD_STOP_REASON_SIGNAL;
    if (strcmp(str, "watchpoint") == 0) return MD_STOP_REASON_WATCHPOINT;
    
    return MD_STOP_REASON_UNKNOWN;
}

/* ============================================================================
 * Breakpoint State Functions
 * ============================================================================ */

const char* md_bp_state_string(md_bp_state_t state) {
    switch (state) {
        case MD_BP_STATE_INVALID:  return "invalid";
        case MD_BP_STATE_PENDING:  return "pending";
        case MD_BP_STATE_VERIFIED: return "verified";
        case MD_BP_STATE_FAILED:   return "failed";
        case MD_BP_STATE_REMOVED:  return "removed";
        case MD_BP_STATE_DISABLED: return "disabled";
        default:                   return "unknown";
    }
}

const char* md_bp_type_string(md_bp_type_t type) {
    switch (type) {
        case MD_BP_TYPE_LINE:         return "line";
        case MD_BP_TYPE_FUNCTION:     return "function";
        case MD_BP_TYPE_CONDITIONAL:  return "conditional";
        case MD_BP_TYPE_DATA:         return "data";
        case MD_BP_TYPE_INSTRUCTION:  return "instruction";
        case MD_BP_TYPE_LOGPOINT:     return "logpoint";
        default:                      return "unknown";
    }
}

/* ============================================================================
 * Thread State Functions
 * ============================================================================ */

const char* md_thread_state_string(md_thread_state_t state) {
    switch (state) {
        case MD_THREAD_STATE_UNKNOWN:  return "unknown";
        case MD_THREAD_STATE_STOPPED:  return "stopped";
        case MD_THREAD_STATE_RUNNING:  return "running";
        case MD_THREAD_STATE_STEPPING: return "stepping";
        case MD_THREAD_STATE_EXITED:   return "exited";
        default:                       return "unknown";
    }
}

/* ============================================================================
 * Scope Type Functions
 * ============================================================================ */

const char* md_scope_type_string(md_scope_type_t type) {
    switch (type) {
        case MD_SCOPE_TYPE_LOCALS:    return "Locals";
        case MD_SCOPE_TYPE_ARGUMENTS: return "Arguments";
        case MD_SCOPE_TYPE_GLOBALS:   return "Globals";
        case MD_SCOPE_TYPE_REGISTERS: return "Registers";
        case MD_SCOPE_TYPE_CLOSURE:   return "Closure";
        case MD_SCOPE_TYPE_RETURN:    return "Return";
        default:                      return "Unknown";
    }
}

/* ============================================================================
 * Variable Kind Functions
 * ============================================================================ */

const char* md_var_kind_string(md_var_kind_t kind) {
    switch (kind) {
        case MD_VAR_KIND_NORMAL:     return "normal";
        case MD_VAR_KIND_POINTER:    return "pointer";
        case MD_VAR_KIND_ARRAY:      return "array";
        case MD_VAR_KIND_OBJECT:     return "object";
        case MD_VAR_KIND_COLLECTION: return "collection";
        case MD_VAR_KIND_STRING:     return "string";
        case MD_VAR_KIND_NULL:       return "null";
        case MD_VAR_KIND_FUNCTION:   return "function";
        case MD_VAR_KIND_VIRTUAL:    return "virtual";
        default:                     return "unknown";
    }
}

/* ============================================================================
 * Module State Functions
 * ============================================================================ */

const char* md_module_state_string(md_module_state_t state) {
    switch (state) {
        case MD_MODULE_STATE_UNKNOWN:  return "unknown";
        case MD_MODULE_STATE_LOADING:  return "loading";
        case MD_MODULE_STATE_LOADED:   return "loaded";
        case MD_MODULE_STATE_FAILED:   return "failed";
        case MD_MODULE_STATE_UNLOADED: return "unloaded";
        default:                       return "unknown";
    }
}

/* ============================================================================
 * Structure Initialization Functions
 * ============================================================================ */

void md_breakpoint_init(md_breakpoint_t *bp) {
    if (bp == NULL) return;
    memset(bp, 0, sizeof(md_breakpoint_t));
    bp->id = MD_INVALID_ID;
    bp->dap_id = MD_INVALID_ID;
    bp->type = MD_BP_TYPE_LINE;
    bp->state = MD_BP_STATE_INVALID;
    bp->line = -1;
    bp->column = 0;
    bp->enabled = true;
    bp->verified = false;
}

void md_breakpoint_clear(md_breakpoint_t *bp) {
    if (bp == NULL) return;
    md_breakpoint_init(bp);
}

void md_thread_init(md_thread_t *thread) {
    if (thread == NULL) return;
    memset(thread, 0, sizeof(md_thread_t));
    thread->id = MD_INVALID_ID;
    thread->state = MD_THREAD_STATE_UNKNOWN;
    thread->current_frame_id = MD_INVALID_ID;
    thread->is_stopped = false;
    thread->is_main = false;
}

void md_stack_frame_init(md_stack_frame_t *frame) {
    if (frame == NULL) return;
    memset(frame, 0, sizeof(md_stack_frame_t));
    frame->id = MD_INVALID_ID;
    frame->line = -1;
    frame->column = 0;
    frame->end_line = 0;
    frame->end_column = 0;
    frame->instruction_pointer = 0;
    frame->return_address = 0;
    frame->thread_id = MD_INVALID_ID;
    frame->is_user_code = true;
    strcpy(frame->presentation_hint, "normal");
}

void md_variable_init(md_variable_t *var) {
    if (var == NULL) return;
    memset(var, 0, sizeof(md_variable_t));
    var->id = MD_INVALID_ID;
    var->variables_reference = 0;
    var->named_children = 0;
    var->indexed_children = 0;
    var->parent_id = 0;
    var->kind = MD_VAR_KIND_NORMAL;
    var->memory_reference = 0;
    var->scope_id = MD_INVALID_ID;
}

void md_scope_init(md_scope_t *scope) {
    if (scope == NULL) return;
    memset(scope, 0, sizeof(md_scope_t));
    scope->id = MD_INVALID_ID;
    scope->type = MD_SCOPE_TYPE_LOCALS;
    scope->frame_id = MD_INVALID_ID;
    scope->named_variables = 0;
    scope->indexed_variables = 0;
    scope->expensive = false;
}

void md_module_init(md_module_t *module) {
    if (module == NULL) return;
    memset(module, 0, sizeof(md_module_t));
    module->id = MD_INVALID_ID;
    module->state = MD_MODULE_STATE_UNKNOWN;
    module->symbols_loaded = false;
    module->base_address = 0;
    module->size = 0;
    module->is_optimized = false;
    module->is_user_code = true;
    module->is_system = false;
}

void md_exception_init(md_exception_t *exc) {
    if (exc == NULL) return;
    memset(exc, 0, sizeof(md_exception_t));
    exc->thread_id = MD_INVALID_ID;
    exc->frame_id = MD_INVALID_ID;
    exc->line = -1;
    exc->is_uncaught = false;
}

void md_debug_state_init(md_debug_state_t *state) {
    if (state == NULL) return;
    memset(state, 0, sizeof(md_debug_state_t));
    state->exec_state = MD_EXEC_STATE_NONE;
    state->stop_reason = MD_STOP_REASON_UNKNOWN;
    state->current_thread_id = MD_INVALID_ID;
    state->current_frame_id = MD_INVALID_ID;
    state->pid = -1;
    state->exit_code = -1;
    state->has_exception = false;
    md_exception_init(&state->exception);
}

void md_debug_state_clear(md_debug_state_t *state) {
    if (state == NULL) return;
    md_debug_state_init(state);
}
