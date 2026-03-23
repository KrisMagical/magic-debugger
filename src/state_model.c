/**
 * @file state_model.c
 * @brief Implementation of Unified Debug State Model
 *
 * This file implements the state model that provides a unified abstraction
 * layer over different debug adapters, shielding the frontend from
 * debugger-specific differences.
 *
 * Phase 3: State Model - Unified Debug State Representation
 */

#include "state_model.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

/**
 * @brief Internal breakpoint entry
 */
typedef struct md_bp_entry {
    md_breakpoint_t bp;
    bool in_use;
    struct md_bp_entry *next;
} md_bp_entry_t;

/**
 * @brief Internal thread entry
 */
typedef struct md_thread_entry {
    md_thread_t thread;
    bool in_use;
    struct md_thread_entry *next;
} md_thread_entry_t;

/**
 * @brief Stack frame list for a thread
 */
typedef struct md_frame_list {
    int thread_id;
    md_stack_frame_t *frames;
    int count;
    int capacity;
    struct md_frame_list *next;
} md_frame_list_t;

/**
 * @brief Scope list for a frame
 */
typedef struct md_scope_list {
    int frame_id;
    md_scope_t *scopes;
    int count;
    int capacity;
    struct md_scope_list *next;
} md_scope_list_t;

/**
 * @brief Variable list for a scope
 */
typedef struct md_var_list {
    int scope_id;
    md_variable_t *variables;
    int count;
    int capacity;
    struct md_var_list *next;
} md_var_list_t;

/**
 * @brief Internal module entry
 */
typedef struct md_module_entry {
    md_module_t module;
    bool in_use;
    struct md_module_entry *next;
} md_module_entry_t;

/**
 * @brief Internal listener entry
 */
typedef struct md_listener_entry {
    md_state_listener_t listener;
    struct md_listener_entry *next;
} md_listener_entry_t;

/**
 * @brief State model internal structure
 */
struct md_state_model {
    /* Configuration */
    md_state_config_t config;

    /* Execution State */
    md_exec_state_t exec_state;
    md_stop_reason_t stop_reason;
    char stop_description[256];
    int current_thread_id;
    int current_frame_id;

    /* Debug State Snapshot */
    md_debug_state_t debug_state;

    /* Breakpoints */
    md_bp_entry_t *breakpoints;
    md_bp_entry_t *bp_pool;
    int bp_count;
    int next_bp_id;

    /* Threads */
    md_thread_entry_t *threads;
    md_thread_entry_t *thread_pool;
    int thread_count;

    /* Stack Frames (per thread) */
    md_frame_list_t *frame_lists;

    /* Scopes (per frame) */
    md_scope_list_t *scope_lists;

    /* Variables (per scope) */
    md_var_list_t *var_lists;

    /* Modules */
    md_module_entry_t *modules;
    md_module_entry_t *module_pool;
    int module_count;

    /* Exception */
    md_exception_t exception;
    bool has_exception;

    /* Listeners */
    md_listener_entry_t *listeners;

    /* Capabilities */
    dap_capabilities_t capabilities;
    bool capabilities_set;
};

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

uint64_t md_state_get_timestamp_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

static void safe_strncpy(char *dest, const char *src, size_t dest_size) {
    if (dest == NULL || dest_size == 0) return;
    if (src == NULL) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

void md_state_config_init(md_state_config_t *config) {
    if (config == NULL) return;
    memset(config, 0, sizeof(md_state_config_t));
    config->max_breakpoints = MD_MAX_BREAKPOINTS;
    config->max_threads = MD_MAX_THREADS;
    config->max_stack_frames = MD_MAX_STACK_FRAMES;
    config->max_variables = MD_MAX_VARIABLES;
    config->max_modules = MD_MAX_MODULES;
    config->auto_update_threads = true;
    config->auto_update_stack = true;
    config->auto_update_variables = false;
}

/* ============================================================================
 * State Model Lifecycle
 * ============================================================================ */

md_state_model_t* md_state_model_create(const md_state_config_t *config) {
    md_state_model_t *model = (md_state_model_t*)calloc(1, sizeof(md_state_model_t));
    if (model == NULL) {
        MD_ERROR("Failed to allocate state model");
        return NULL;
    }

    /* Apply configuration */
    if (config != NULL) {
        memcpy(&model->config, config, sizeof(md_state_config_t));
    } else {
        md_state_config_init(&model->config);
    }

    /* Initialize state */
    model->exec_state = MD_EXEC_STATE_NONE;
    model->stop_reason = MD_STOP_REASON_UNKNOWN;
    model->current_thread_id = MD_INVALID_ID;
    model->current_frame_id = MD_INVALID_ID;
    model->next_bp_id = 1;
    model->bp_count = 0;
    model->thread_count = 0;
    model->module_count = 0;
    model->has_exception = false;

    /* Initialize debug state */
    md_debug_state_init(&model->debug_state);

    /* Initialize exception */
    md_exception_init(&model->exception);

    MD_INFO("State model created");
    return model;
}

void md_state_model_destroy(md_state_model_t *model) {
    if (model == NULL) return;

    /* Free breakpoints */
    md_bp_entry_t *bp = model->breakpoints;
    while (bp != NULL) {
        md_bp_entry_t *next = bp->next;
        free(bp);
        bp = next;
    }
    bp = model->bp_pool;
    while (bp != NULL) {
        md_bp_entry_t *next = bp->next;
        free(bp);
        bp = next;
    }

    /* Free threads */
    md_thread_entry_t *thr = model->threads;
    while (thr != NULL) {
        md_thread_entry_t *next = thr->next;
        free(thr);
        thr = next;
    }
    thr = model->thread_pool;
    while (thr != NULL) {
        md_thread_entry_t *next = thr->next;
        free(thr);
        thr = next;
    }

    /* Free frame lists */
    md_frame_list_t *fl = model->frame_lists;
    while (fl != NULL) {
        md_frame_list_t *next = fl->next;
        free(fl->frames);
        free(fl);
        fl = next;
    }

    /* Free scope lists */
    md_scope_list_t *sl = model->scope_lists;
    while (sl != NULL) {
        md_scope_list_t *next = sl->next;
        free(sl->scopes);
        free(sl);
        sl = next;
    }

    /* Free variable lists */
    md_var_list_t *vl = model->var_lists;
    while (vl != NULL) {
        md_var_list_t *next = vl->next;
        free(vl->variables);
        free(vl);
        vl = next;
    }

    /* Free modules */
    md_module_entry_t *mod = model->modules;
    while (mod != NULL) {
        md_module_entry_t *next = mod->next;
        free(mod);
        mod = next;
    }
    mod = model->module_pool;
    while (mod != NULL) {
        md_module_entry_t *next = mod->next;
        free(mod);
        mod = next;
    }

    /* Free listeners */
    md_listener_entry_t *ln = model->listeners;
    while (ln != NULL) {
        md_listener_entry_t *next = ln->next;
        free(ln);
        ln = next;
    }

    /* Free capabilities */
    if (model->capabilities_set) {
        dap_capabilities_free(&model->capabilities);
    }

    free(model);
    MD_INFO("State model destroyed");
}

md_error_t md_state_model_reset(md_state_model_t *model) {
    if (model == NULL) return MD_ERROR_INVALID_ARG;

    /* Reset execution state */
    model->exec_state = MD_EXEC_STATE_NONE;
    model->stop_reason = MD_STOP_REASON_UNKNOWN;
    model->stop_description[0] = '\0';
    model->current_thread_id = MD_INVALID_ID;
    model->current_frame_id = MD_INVALID_ID;

    /* Clear breakpoints */
    md_bp_entry_t *bp = model->breakpoints;
    while (bp != NULL) {
        md_bp_entry_t *next = bp->next;
        bp->in_use = false;
        bp->next = model->bp_pool;
        model->bp_pool = bp;
        bp = next;
    }
    model->breakpoints = NULL;
    model->bp_count = 0;

    /* Clear threads */
    md_thread_entry_t *thr = model->threads;
    while (thr != NULL) {
        md_thread_entry_t *next = thr->next;
        thr->in_use = false;
        thr->next = model->thread_pool;
        model->thread_pool = thr;
        thr = next;
    }
    model->threads = NULL;
    model->thread_count = 0;

    /* Clear frame lists */
    md_frame_list_t *fl = model->frame_lists;
    while (fl != NULL) {
        md_frame_list_t *next = fl->next;
        free(fl->frames);
        free(fl);
        fl = next;
    }
    model->frame_lists = NULL;

    /* Clear scope lists */
    md_scope_list_t *sl = model->scope_lists;
    while (sl != NULL) {
        md_scope_list_t *next = sl->next;
        free(sl->scopes);
        free(sl);
        sl = next;
    }
    model->scope_lists = NULL;

    /* Clear variable lists */
    md_var_list_t *vl = model->var_lists;
    while (vl != NULL) {
        md_var_list_t *next = vl->next;
        free(vl->variables);
        free(vl);
        vl = next;
    }
    model->var_lists = NULL;

    /* Clear modules */
    md_module_entry_t *mod = model->modules;
    while (mod != NULL) {
        md_module_entry_t *next = mod->next;
        mod->in_use = false;
        mod->next = model->module_pool;
        model->module_pool = mod;
        mod = next;
    }
    model->modules = NULL;
    model->module_count = 0;

    /* Clear exception */
    md_exception_init(&model->exception);
    model->has_exception = false;

    /* Reset debug state */
    md_debug_state_init(&model->debug_state);

    MD_INFO("State model reset");
    return MD_SUCCESS;
}

/* ============================================================================
 * State Change Notification
 * ============================================================================ */

md_error_t md_state_model_add_listener(md_state_model_t *model,
                                        const md_state_listener_t *listener) {
    if (model == NULL || listener == NULL || listener->callback == NULL) {
        return MD_ERROR_INVALID_ARG;
    }

    md_listener_entry_t *entry = (md_listener_entry_t*)malloc(sizeof(md_listener_entry_t));
    if (entry == NULL) {
        return MD_ERROR_OUT_OF_MEMORY;
    }

    memcpy(&entry->listener, listener, sizeof(md_state_listener_t));
    entry->next = model->listeners;
    model->listeners = entry;

    return MD_SUCCESS;
}

md_error_t md_state_model_remove_listener(md_state_model_t *model,
                                           md_state_callback_t callback) {
    if (model == NULL || callback == NULL) {
        return MD_ERROR_INVALID_ARG;
    }

    md_listener_entry_t **pp = &model->listeners;
    while (*pp != NULL) {
        if ((*pp)->listener.callback == callback) {
            md_listener_entry_t *entry = *pp;
            *pp = entry->next;
            free(entry);
            return MD_SUCCESS;
        }
        pp = &(*pp)->next;
    }

    return MD_ERROR_NOT_FOUND;
}

void md_state_model_notify(md_state_model_t *model,
                           md_state_event_t event,
                           void *data) {
    if (model == NULL) return;

    md_listener_entry_t *entry = model->listeners;
    while (entry != NULL) {
        if (entry->listener.events & event) {
            entry->listener.callback(model, event, data, entry->listener.user_data);
        }
        entry = entry->next;
    }
}

/* ============================================================================
 * Execution State Management
 * ============================================================================ */

md_exec_state_t md_state_get_exec_state(md_state_model_t *model) {
    return model != NULL ? model->exec_state : MD_EXEC_STATE_NONE;
}

md_error_t md_state_set_exec_state(md_state_model_t *model,
                                   md_exec_state_t state) {
    if (model == NULL) return MD_ERROR_INVALID_ARG;

    md_exec_state_t old_state = model->exec_state;
    model->exec_state = state;
    model->debug_state.exec_state = state;
    model->debug_state.state_timestamp = md_state_get_timestamp_ms();

    /* Update timestamps */
    if (state == MD_EXEC_STATE_RUNNING && old_state != MD_EXEC_STATE_RUNNING) {
        model->debug_state.last_run_timestamp = model->debug_state.state_timestamp;
    } else if (md_exec_state_is_stopped(state) && !md_exec_state_is_stopped(old_state)) {
        model->debug_state.last_stop_timestamp = model->debug_state.state_timestamp;
        model->debug_state.total_stops++;
    }

    /* Notify listeners */
    if (old_state != state) {
        md_state_model_notify(model, MD_STATE_EVENT_EXEC_CHANGED, &old_state);
    }

    MD_DEBUG("Execution state changed: %s -> %s",
             md_exec_state_string(old_state), md_exec_state_string(state));
    return MD_SUCCESS;
}

md_stop_reason_t md_state_get_stop_reason(md_state_model_t *model) {
    return model != NULL ? model->stop_reason : MD_STOP_REASON_UNKNOWN;
}

md_error_t md_state_set_stop_reason(md_state_model_t *model,
                                    md_stop_reason_t reason,
                                    const char *description) {
    if (model == NULL) return MD_ERROR_INVALID_ARG;

    model->stop_reason = reason;
    model->debug_state.stop_reason = reason;

    if (description != NULL) {
        safe_strncpy(model->stop_description, description, sizeof(model->stop_description));
        safe_strncpy(model->debug_state.stop_description, description,
                     sizeof(model->debug_state.stop_description));
    } else {
        model->stop_description[0] = '\0';
        model->debug_state.stop_description[0] = '\0';
    }

    return MD_SUCCESS;
}

int md_state_get_current_thread_id(md_state_model_t *model) {
    return model != NULL ? model->current_thread_id : MD_INVALID_ID;
}

md_error_t md_state_set_current_thread_id(md_state_model_t *model, int thread_id) {
    if (model == NULL) return MD_ERROR_INVALID_ARG;
    model->current_thread_id = thread_id;
    model->debug_state.current_thread_id = thread_id;
    return MD_SUCCESS;
}

int md_state_get_current_frame_id(md_state_model_t *model) {
    return model != NULL ? model->current_frame_id : MD_INVALID_ID;
}

md_error_t md_state_set_current_frame_id(md_state_model_t *model, int frame_id) {
    if (model == NULL) return MD_ERROR_INVALID_ARG;
    model->current_frame_id = frame_id;
    model->debug_state.current_frame_id = frame_id;
    return MD_SUCCESS;
}

md_error_t md_state_get_debug_state(md_state_model_t *model,
                                    md_debug_state_t *state) {
    if (model == NULL || state == NULL) return MD_ERROR_INVALID_ARG;
    memcpy(state, &model->debug_state, sizeof(md_debug_state_t));
    return MD_SUCCESS;
}

/* ============================================================================
 * Breakpoint Management
 * ============================================================================ */

int md_state_add_breakpoint(md_state_model_t *model, md_breakpoint_t *bp) {
    if (model == NULL || bp == NULL) return MD_ERROR_INVALID_ARG;

    /* Check capacity */
    if (model->bp_count >= model->config.max_breakpoints) {
        MD_ERROR("Maximum breakpoints reached");
        return MD_ERROR_BUFFER_OVERFLOW;
    }

    /* Allocate entry */
    md_bp_entry_t *entry = model->bp_pool;
    if (entry != NULL) {
        model->bp_pool = entry->next;
    } else {
        entry = (md_bp_entry_t*)malloc(sizeof(md_bp_entry_t));
        if (entry == NULL) return MD_ERROR_OUT_OF_MEMORY;
    }

    /* Initialize */
    memset(entry, 0, sizeof(md_bp_entry_t));
    entry->in_use = true;
    memcpy(&entry->bp, bp, sizeof(md_breakpoint_t));

    /* Assign ID */
    entry->bp.id = model->next_bp_id++;
    bp->id = entry->bp.id;

    /* Add to list */
    entry->next = model->breakpoints;
    model->breakpoints = entry;
    model->bp_count++;

    MD_DEBUG("Added breakpoint %d at %s:%d", entry->bp.id, entry->bp.path, entry->bp.line);

    /* Notify */
    md_state_model_notify(model, MD_STATE_EVENT_BREAKPOINT_CHANGED, &entry->bp);

    return entry->bp.id;
}

md_error_t md_state_update_breakpoint(md_state_model_t *model,
                                      const md_breakpoint_t *bp) {
    if (model == NULL || bp == NULL) return MD_ERROR_INVALID_ARG;

    md_bp_entry_t *entry = model->breakpoints;
    while (entry != NULL) {
        if (entry->bp.id == bp->id) {
            /* Preserve adapter_data */
            void *adapter_data = entry->bp.adapter_data;
            memcpy(&entry->bp, bp, sizeof(md_breakpoint_t));
            entry->bp.adapter_data = adapter_data;

            md_state_model_notify(model, MD_STATE_EVENT_BREAKPOINT_CHANGED, &entry->bp);
            return MD_SUCCESS;
        }
        entry = entry->next;
    }

    return MD_ERROR_NOT_FOUND;
}

md_error_t md_state_remove_breakpoint(md_state_model_t *model, int id) {
    if (model == NULL) return MD_ERROR_INVALID_ARG;

    md_bp_entry_t **pp = &model->breakpoints;
    while (*pp != NULL) {
        if ((*pp)->bp.id == id) {
            md_bp_entry_t *entry = *pp;
            *pp = entry->next;
            entry->in_use = false;
            entry->next = model->bp_pool;
            model->bp_pool = entry;
            model->bp_count--;

            md_state_model_notify(model, MD_STATE_EVENT_BREAKPOINT_CHANGED, NULL);
            MD_DEBUG("Removed breakpoint %d", id);
            return MD_SUCCESS;
        }
        pp = &(*pp)->next;
    }

    return MD_ERROR_NOT_FOUND;
}

md_error_t md_state_get_breakpoint(md_state_model_t *model, int id,
                                   md_breakpoint_t *bp) {
    if (model == NULL || bp == NULL) return MD_ERROR_INVALID_ARG;

    md_bp_entry_t *entry = model->breakpoints;
    while (entry != NULL) {
        if (entry->bp.id == id) {
            memcpy(bp, &entry->bp, sizeof(md_breakpoint_t));
            return MD_SUCCESS;
        }
        entry = entry->next;
    }

    return MD_ERROR_NOT_FOUND;
}

md_error_t md_state_find_breakpoint_by_dap_id(md_state_model_t *model,
                                              int dap_id,
                                              md_breakpoint_t *bp) {
    if (model == NULL || bp == NULL) return MD_ERROR_INVALID_ARG;

    md_bp_entry_t *entry = model->breakpoints;
    while (entry != NULL) {
        if (entry->bp.dap_id == dap_id) {
            memcpy(bp, &entry->bp, sizeof(md_breakpoint_t));
            return MD_SUCCESS;
        }
        entry = entry->next;
    }

    return MD_ERROR_NOT_FOUND;
}

md_error_t md_state_find_breakpoint_by_location(md_state_model_t *model,
                                                const char *path,
                                                int line,
                                                md_breakpoint_t *bp) {
    if (model == NULL || path == NULL || bp == NULL) {
        return MD_ERROR_INVALID_ARG;
    }

    md_bp_entry_t *entry = model->breakpoints;
    while (entry != NULL) {
        if (entry->bp.line == line && strcmp(entry->bp.path, path) == 0) {
            memcpy(bp, &entry->bp, sizeof(md_breakpoint_t));
            return MD_SUCCESS;
        }
        entry = entry->next;
    }

    return MD_ERROR_NOT_FOUND;
}

md_error_t md_state_get_all_breakpoints(md_state_model_t *model,
                                        md_breakpoint_t *breakpoints,
                                        int max_count,
                                        int *actual_count) {
    if (model == NULL || breakpoints == NULL || actual_count == NULL) {
        return MD_ERROR_INVALID_ARG;
    }

    int count = 0;
    md_bp_entry_t *entry = model->breakpoints;
    while (entry != NULL && count < max_count) {
        memcpy(&breakpoints[count], &entry->bp, sizeof(md_breakpoint_t));
        count++;
        entry = entry->next;
    }

    *actual_count = count;
    return MD_SUCCESS;
}

int md_state_get_breakpoint_count(md_state_model_t *model) {
    return model != NULL ? model->bp_count : 0;
}

md_error_t md_state_clear_all_breakpoints(md_state_model_t *model) {
    if (model == NULL) return MD_ERROR_INVALID_ARG;

    md_bp_entry_t *entry = model->breakpoints;
    while (entry != NULL) {
        md_bp_entry_t *next = entry->next;
        entry->in_use = false;
        entry->next = model->bp_pool;
        model->bp_pool = entry;
        entry = next;
    }

    model->breakpoints = NULL;
    model->bp_count = 0;

    md_state_model_notify(model, MD_STATE_EVENT_BREAKPOINT_CHANGED, NULL);
    return MD_SUCCESS;
}

md_error_t md_state_set_breakpoint_enabled(md_state_model_t *model,
                                           int id, bool enabled) {
    if (model == NULL) return MD_ERROR_INVALID_ARG;

    md_bp_entry_t *entry = model->breakpoints;
    while (entry != NULL) {
        if (entry->bp.id == id) {
            entry->bp.enabled = enabled;
            entry->bp.state = enabled ? MD_BP_STATE_PENDING : MD_BP_STATE_DISABLED;

            md_state_model_notify(model, MD_STATE_EVENT_BREAKPOINT_CHANGED, &entry->bp);
            return MD_SUCCESS;
        }
        entry = entry->next;
    }

    return MD_ERROR_NOT_FOUND;
}

/* ============================================================================
 * Thread Management
 * ============================================================================ */

md_error_t md_state_add_thread(md_state_model_t *model,
                               const md_thread_t *thread) {
    if (model == NULL || thread == NULL) return MD_ERROR_INVALID_ARG;

    /* Check capacity */
    if (model->thread_count >= model->config.max_threads) {
        return MD_ERROR_BUFFER_OVERFLOW;
    }

    /* Check if thread already exists */
    md_thread_entry_t *entry = model->threads;
    while (entry != NULL) {
        if (entry->thread.id == thread->id) {
            /* Update existing */
            memcpy(&entry->thread, thread, sizeof(md_thread_t));
            return MD_SUCCESS;
        }
        entry = entry->next;
    }

    /* Allocate new entry */
    entry = model->thread_pool;
    if (entry != NULL) {
        model->thread_pool = entry->next;
    } else {
        entry = (md_thread_entry_t*)malloc(sizeof(md_thread_entry_t));
        if (entry == NULL) return MD_ERROR_OUT_OF_MEMORY;
    }

    memset(entry, 0, sizeof(md_thread_entry_t));
    entry->in_use = true;
    memcpy(&entry->thread, thread, sizeof(md_thread_t));
    entry->next = model->threads;
    model->threads = entry;
    model->thread_count++;

    /* Set as current thread if first thread */
    if (model->current_thread_id == MD_INVALID_ID) {
        model->current_thread_id = thread->id;
        model->debug_state.current_thread_id = thread->id;
    }

    md_state_model_notify(model, MD_STATE_EVENT_THREAD_CHANGED, &entry->thread);
    return MD_SUCCESS;
}

md_error_t md_state_update_thread(md_state_model_t *model,
                                  const md_thread_t *thread) {
    if (model == NULL || thread == NULL) return MD_ERROR_INVALID_ARG;

    md_thread_entry_t *entry = model->threads;
    while (entry != NULL) {
        if (entry->thread.id == thread->id) {
            memcpy(&entry->thread, thread, sizeof(md_thread_t));
            md_state_model_notify(model, MD_STATE_EVENT_THREAD_CHANGED, &entry->thread);
            return MD_SUCCESS;
        }
        entry = entry->next;
    }

    return MD_ERROR_NOT_FOUND;
}

md_error_t md_state_remove_thread(md_state_model_t *model, int thread_id) {
    if (model == NULL) return MD_ERROR_INVALID_ARG;

    md_thread_entry_t **pp = &model->threads;
    while (*pp != NULL) {
        if ((*pp)->thread.id == thread_id) {
            md_thread_entry_t *entry = *pp;
            *pp = entry->next;
            entry->in_use = false;
            entry->next = model->thread_pool;
            model->thread_pool = entry;
            model->thread_count--;

            /* Update current thread if needed */
            if (model->current_thread_id == thread_id) {
                model->current_thread_id = model->threads ? model->threads->thread.id : MD_INVALID_ID;
            }

            md_state_model_notify(model, MD_STATE_EVENT_THREAD_CHANGED, NULL);
            return MD_SUCCESS;
        }
        pp = &(*pp)->next;
    }

    return MD_ERROR_NOT_FOUND;
}

md_error_t md_state_get_thread(md_state_model_t *model, int thread_id,
                               md_thread_t *thread) {
    if (model == NULL || thread == NULL) return MD_ERROR_INVALID_ARG;

    md_thread_entry_t *entry = model->threads;
    while (entry != NULL) {
        if (entry->thread.id == thread_id) {
            memcpy(thread, &entry->thread, sizeof(md_thread_t));
            return MD_SUCCESS;
        }
        entry = entry->next;
    }

    return MD_ERROR_NOT_FOUND;
}

md_error_t md_state_get_all_threads(md_state_model_t *model,
                                    md_thread_t *threads,
                                    int max_count,
                                    int *actual_count) {
    if (model == NULL || threads == NULL || actual_count == NULL) {
        return MD_ERROR_INVALID_ARG;
    }

    int count = 0;
    md_thread_entry_t *entry = model->threads;
    while (entry != NULL && count < max_count) {
        memcpy(&threads[count], &entry->thread, sizeof(md_thread_t));
        count++;
        entry = entry->next;
    }

    *actual_count = count;
    return MD_SUCCESS;
}

int md_state_get_thread_count(md_state_model_t *model) {
    return model != NULL ? model->thread_count : 0;
}

md_error_t md_state_clear_all_threads(md_state_model_t *model) {
    if (model == NULL) return MD_ERROR_INVALID_ARG;

    md_thread_entry_t *entry = model->threads;
    while (entry != NULL) {
        md_thread_entry_t *next = entry->next;
        entry->in_use = false;
        entry->next = model->thread_pool;
        model->thread_pool = entry;
        entry = next;
    }

    model->threads = NULL;
    model->thread_count = 0;
    model->current_thread_id = MD_INVALID_ID;

    return MD_SUCCESS;
}

/* ============================================================================
 * Stack Frame Management
 * ============================================================================ */

static md_frame_list_t* find_or_create_frame_list(md_state_model_t *model, int thread_id) {
    md_frame_list_t *fl = model->frame_lists;
    while (fl != NULL) {
        if (fl->thread_id == thread_id) return fl;
        fl = fl->next;
    }

    /* Create new */
    fl = (md_frame_list_t*)calloc(1, sizeof(md_frame_list_t));
    if (fl == NULL) return NULL;

    fl->thread_id = thread_id;
    fl->capacity = model->config.max_stack_frames;
    fl->frames = (md_stack_frame_t*)calloc(fl->capacity, sizeof(md_stack_frame_t));
    if (fl->frames == NULL) {
        free(fl);
        return NULL;
    }

    fl->next = model->frame_lists;
    model->frame_lists = fl;
    return fl;
}

md_error_t md_state_set_stack_frames(md_state_model_t *model,
                                     int thread_id,
                                     const md_stack_frame_t *frames,
                                     int count) {
    if (model == NULL || (frames == NULL && count > 0)) {
        return MD_ERROR_INVALID_ARG;
    }

    md_frame_list_t *fl = find_or_create_frame_list(model, thread_id);
    if (fl == NULL) return MD_ERROR_OUT_OF_MEMORY;

    /* Clear existing */
    if (fl->frames != NULL) {
        free(fl->frames);
    }

    /* Allocate new */
    fl->capacity = count > model->config.max_stack_frames ? count : model->config.max_stack_frames;
    fl->frames = (md_stack_frame_t*)calloc(fl->capacity, sizeof(md_stack_frame_t));
    if (fl->frames == NULL) {
        fl->count = 0;
        fl->capacity = 0;
        return MD_ERROR_OUT_OF_MEMORY;
    }

    memcpy(fl->frames, frames, count * sizeof(md_stack_frame_t));
    fl->count = count;

    md_state_model_notify(model, MD_STATE_EVENT_STACK_CHANGED, &thread_id);
    return MD_SUCCESS;
}

md_error_t md_state_get_stack_frames(md_state_model_t *model,
                                     int thread_id,
                                     md_stack_frame_t *frames,
                                     int max_count,
                                     int *actual_count) {
    if (model == NULL || frames == NULL || actual_count == NULL) {
        return MD_ERROR_INVALID_ARG;
    }

    md_frame_list_t *fl = model->frame_lists;
    while (fl != NULL) {
        if (fl->thread_id == thread_id) {
            int count = fl->count < max_count ? fl->count : max_count;
            memcpy(frames, fl->frames, count * sizeof(md_stack_frame_t));
            *actual_count = count;
            return MD_SUCCESS;
        }
        fl = fl->next;
    }

    *actual_count = 0;
    return MD_ERROR_NOT_FOUND;
}

md_error_t md_state_get_stack_frame(md_state_model_t *model,
                                    int frame_id,
                                    md_stack_frame_t *frame) {
    if (model == NULL || frame == NULL) return MD_ERROR_INVALID_ARG;

    md_frame_list_t *fl = model->frame_lists;
    while (fl != NULL) {
        for (int i = 0; i < fl->count; i++) {
            if (fl->frames[i].id == frame_id) {
                memcpy(frame, &fl->frames[i], sizeof(md_stack_frame_t));
                return MD_SUCCESS;
            }
        }
        fl = fl->next;
    }

    return MD_ERROR_NOT_FOUND;
}

int md_state_get_stack_frame_count(md_state_model_t *model, int thread_id) {
    if (model == NULL) return 0;

    md_frame_list_t *fl = model->frame_lists;
    while (fl != NULL) {
        if (fl->thread_id == thread_id) {
            return fl->count;
        }
        fl = fl->next;
    }

    return 0;
}

md_error_t md_state_clear_stack_frames(md_state_model_t *model, int thread_id) {
    if (model == NULL) return MD_ERROR_INVALID_ARG;

    md_frame_list_t **pp = &model->frame_lists;
    while (*pp != NULL) {
        if ((*pp)->thread_id == thread_id) {
            md_frame_list_t *fl = *pp;
            *pp = fl->next;
            free(fl->frames);
            free(fl);
            return MD_SUCCESS;
        }
        pp = &(*pp)->next;
    }

    return MD_ERROR_NOT_FOUND;
}

md_error_t md_state_clear_all_stack_frames(md_state_model_t *model) {
    if (model == NULL) return MD_ERROR_INVALID_ARG;

    md_frame_list_t *fl = model->frame_lists;
    while (fl != NULL) {
        md_frame_list_t *next = fl->next;
        free(fl->frames);
        free(fl);
        fl = next;
    }

    model->frame_lists = NULL;
    return MD_SUCCESS;
}

/* ============================================================================
 * Variable/Scope Management
 * ============================================================================ */

static md_scope_list_t* find_or_create_scope_list(md_state_model_t *model, int frame_id) {
    md_scope_list_t *sl = model->scope_lists;
    while (sl != NULL) {
        if (sl->frame_id == frame_id) return sl;
        sl = sl->next;
    }

    sl = (md_scope_list_t*)calloc(1, sizeof(md_scope_list_t));
    if (sl == NULL) return NULL;

    sl->frame_id = frame_id;
    sl->capacity = MD_MAX_SCOPES;
    sl->scopes = (md_scope_t*)calloc(sl->capacity, sizeof(md_scope_t));
    if (sl->scopes == NULL) {
        free(sl);
        return NULL;
    }

    sl->next = model->scope_lists;
    model->scope_lists = sl;
    return sl;
}

static md_var_list_t* find_or_create_var_list(md_state_model_t *model, int scope_id) {
    md_var_list_t *vl = model->var_lists;
    while (vl != NULL) {
        if (vl->scope_id == scope_id) return vl;
        vl = vl->next;
    }

    vl = (md_var_list_t*)calloc(1, sizeof(md_var_list_t));
    if (vl == NULL) return NULL;

    vl->scope_id = scope_id;
    vl->capacity = model->config.max_variables;
    vl->variables = (md_variable_t*)calloc(vl->capacity, sizeof(md_variable_t));
    if (vl->variables == NULL) {
        free(vl);
        return NULL;
    }

    vl->next = model->var_lists;
    model->var_lists = vl;
    return vl;
}

md_error_t md_state_set_scopes(md_state_model_t *model,
                               int frame_id,
                               const md_scope_t *scopes,
                               int count) {
    if (model == NULL || (scopes == NULL && count > 0)) {
        return MD_ERROR_INVALID_ARG;
    }

    md_scope_list_t *sl = find_or_create_scope_list(model, frame_id);
    if (sl == NULL) return MD_ERROR_OUT_OF_MEMORY;

    if (sl->scopes != NULL) {
        free(sl->scopes);
    }

    sl->capacity = count > MD_MAX_SCOPES ? count : MD_MAX_SCOPES;
    sl->scopes = (md_scope_t*)calloc(sl->capacity, sizeof(md_scope_t));
    if (sl->scopes == NULL) {
        sl->count = 0;
        sl->capacity = 0;
        return MD_ERROR_OUT_OF_MEMORY;
    }

    memcpy(sl->scopes, scopes, count * sizeof(md_scope_t));
    sl->count = count;

    return MD_SUCCESS;
}

md_error_t md_state_get_scopes(md_state_model_t *model,
                               int frame_id,
                               md_scope_t *scopes,
                               int max_count,
                               int *actual_count) {
    if (model == NULL || scopes == NULL || actual_count == NULL) {
        return MD_ERROR_INVALID_ARG;
    }

    md_scope_list_t *sl = model->scope_lists;
    while (sl != NULL) {
        if (sl->frame_id == frame_id) {
            int count = sl->count < max_count ? sl->count : max_count;
            memcpy(scopes, sl->scopes, count * sizeof(md_scope_t));
            *actual_count = count;
            return MD_SUCCESS;
        }
        sl = sl->next;
    }

    *actual_count = 0;
    return MD_ERROR_NOT_FOUND;
}

md_error_t md_state_set_variables(md_state_model_t *model,
                                  int scope_id,
                                  const md_variable_t *variables,
                                  int count) {
    if (model == NULL || (variables == NULL && count > 0)) {
        return MD_ERROR_INVALID_ARG;
    }

    md_var_list_t *vl = find_or_create_var_list(model, scope_id);
    if (vl == NULL) return MD_ERROR_OUT_OF_MEMORY;

    if (vl->variables != NULL) {
        free(vl->variables);
    }

    vl->capacity = count > model->config.max_variables ? count : model->config.max_variables;
    vl->variables = (md_variable_t*)calloc(vl->capacity, sizeof(md_variable_t));
    if (vl->variables == NULL) {
        vl->count = 0;
        vl->capacity = 0;
        return MD_ERROR_OUT_OF_MEMORY;
    }

    memcpy(vl->variables, variables, count * sizeof(md_variable_t));
    vl->count = count;

    md_state_model_notify(model, MD_STATE_EVENT_VARIABLE_CHANGED, &scope_id);
    return MD_SUCCESS;
}

md_error_t md_state_get_variables(md_state_model_t *model,
                                  int scope_id,
                                  md_variable_t *variables,
                                  int max_count,
                                  int *actual_count) {
    if (model == NULL || variables == NULL || actual_count == NULL) {
        return MD_ERROR_INVALID_ARG;
    }

    md_var_list_t *vl = model->var_lists;
    while (vl != NULL) {
        if (vl->scope_id == scope_id) {
            int count = vl->count < max_count ? vl->count : max_count;
            memcpy(variables, vl->variables, count * sizeof(md_variable_t));
            *actual_count = count;
            return MD_SUCCESS;
        }
        vl = vl->next;
    }

    *actual_count = 0;
    return MD_ERROR_NOT_FOUND;
}

md_error_t md_state_get_variable(md_state_model_t *model,
                                 int var_id,
                                 md_variable_t *variable) {
    if (model == NULL || variable == NULL) return MD_ERROR_INVALID_ARG;

    md_var_list_t *vl = model->var_lists;
    while (vl != NULL) {
        for (int i = 0; i < vl->count; i++) {
            if (vl->variables[i].id == var_id) {
                memcpy(variable, &vl->variables[i], sizeof(md_variable_t));
                return MD_SUCCESS;
            }
        }
        vl = vl->next;
    }

    return MD_ERROR_NOT_FOUND;
}

md_error_t md_state_clear_all_variables(md_state_model_t *model) {
    if (model == NULL) return MD_ERROR_INVALID_ARG;

    md_var_list_t *vl = model->var_lists;
    while (vl != NULL) {
        md_var_list_t *next = vl->next;
        free(vl->variables);
        free(vl);
        vl = next;
    }

    model->var_lists = NULL;
    return MD_SUCCESS;
}

/* ============================================================================
 * Module Management
 * ============================================================================ */

md_error_t md_state_add_module(md_state_model_t *model,
                               const md_module_t *module) {
    if (model == NULL || module == NULL) return MD_ERROR_INVALID_ARG;

    if (model->module_count >= model->config.max_modules) {
        return MD_ERROR_BUFFER_OVERFLOW;
    }

    /* Check if module exists */
    md_module_entry_t *entry = model->modules;
    while (entry != NULL) {
        if (entry->module.id == module->id) {
            memcpy(&entry->module, module, sizeof(md_module_t));
            return MD_SUCCESS;
        }
        entry = entry->next;
    }

    /* Allocate */
    entry = model->module_pool;
    if (entry != NULL) {
        model->module_pool = entry->next;
    } else {
        entry = (md_module_entry_t*)malloc(sizeof(md_module_entry_t));
        if (entry == NULL) return MD_ERROR_OUT_OF_MEMORY;
    }

    memset(entry, 0, sizeof(md_module_entry_t));
    entry->in_use = true;
    memcpy(&entry->module, module, sizeof(md_module_t));
    entry->next = model->modules;
    model->modules = entry;
    model->module_count++;

    md_state_model_notify(model, MD_STATE_EVENT_MODULE_CHANGED, &entry->module);
    return MD_SUCCESS;
}

md_error_t md_state_update_module(md_state_model_t *model,
                                  const md_module_t *module) {
    if (model == NULL || module == NULL) return MD_ERROR_INVALID_ARG;

    md_module_entry_t *entry = model->modules;
    while (entry != NULL) {
        if (entry->module.id == module->id) {
            memcpy(&entry->module, module, sizeof(md_module_t));
            md_state_model_notify(model, MD_STATE_EVENT_MODULE_CHANGED, &entry->module);
            return MD_SUCCESS;
        }
        entry = entry->next;
    }

    return MD_ERROR_NOT_FOUND;
}

md_error_t md_state_remove_module(md_state_model_t *model, int module_id) {
    if (model == NULL) return MD_ERROR_INVALID_ARG;

    md_module_entry_t **pp = &model->modules;
    while (*pp != NULL) {
        if ((*pp)->module.id == module_id) {
            md_module_entry_t *entry = *pp;
            *pp = entry->next;
            entry->in_use = false;
            entry->next = model->module_pool;
            model->module_pool = entry;
            model->module_count--;
            return MD_SUCCESS;
        }
        pp = &(*pp)->next;
    }

    return MD_ERROR_NOT_FOUND;
}

md_error_t md_state_get_module(md_state_model_t *model, int module_id,
                               md_module_t *module) {
    if (model == NULL || module == NULL) return MD_ERROR_INVALID_ARG;

    md_module_entry_t *entry = model->modules;
    while (entry != NULL) {
        if (entry->module.id == module_id) {
            memcpy(module, &entry->module, sizeof(md_module_t));
            return MD_SUCCESS;
        }
        entry = entry->next;
    }

    return MD_ERROR_NOT_FOUND;
}

md_error_t md_state_get_all_modules(md_state_model_t *model,
                                    md_module_t *modules,
                                    int max_count,
                                    int *actual_count) {
    if (model == NULL || modules == NULL || actual_count == NULL) {
        return MD_ERROR_INVALID_ARG;
    }

    int count = 0;
    md_module_entry_t *entry = model->modules;
    while (entry != NULL && count < max_count) {
        memcpy(&modules[count], &entry->module, sizeof(md_module_t));
        count++;
        entry = entry->next;
    }

    *actual_count = count;
    return MD_SUCCESS;
}

int md_state_get_module_count(md_state_model_t *model) {
    return model != NULL ? model->module_count : 0;
}

/* ============================================================================
 * Exception Management
 * ============================================================================ */

md_error_t md_state_set_exception(md_state_model_t *model,
                                  const md_exception_t *exception) {
    if (model == NULL || exception == NULL) return MD_ERROR_INVALID_ARG;

    memcpy(&model->exception, exception, sizeof(md_exception_t));
    model->has_exception = true;
    model->debug_state.has_exception = true;
    memcpy(&model->debug_state.exception, exception, sizeof(md_exception_t));

    md_state_model_notify(model, MD_STATE_EVENT_EXCEPTION_CHANGED, &model->exception);
    return MD_SUCCESS;
}

md_error_t md_state_get_exception(md_state_model_t *model,
                                  md_exception_t *exception) {
    if (model == NULL || exception == NULL) return MD_ERROR_INVALID_ARG;

    if (!model->has_exception) {
        return MD_ERROR_NOT_FOUND;
    }

    memcpy(exception, &model->exception, sizeof(md_exception_t));
    return MD_SUCCESS;
}

md_error_t md_state_clear_exception(md_state_model_t *model) {
    if (model == NULL) return MD_ERROR_INVALID_ARG;

    md_exception_init(&model->exception);
    model->has_exception = false;
    model->debug_state.has_exception = false;

    md_state_model_notify(model, MD_STATE_EVENT_EXCEPTION_CHANGED, NULL);
    return MD_SUCCESS;
}

bool md_state_has_exception(md_state_model_t *model) {
    return model != NULL ? model->has_exception : false;
}

/* ============================================================================
 * DAP Event Translation
 * ============================================================================ */

md_error_t md_state_handle_stopped_event(md_state_model_t *model,
                                         const cJSON *body) {
    if (model == NULL || body == NULL) return MD_ERROR_INVALID_ARG;

    /* Parse reason */
    cJSON *reason = cJSON_GetObjectItemCaseSensitive(body, "reason");
    if (reason != NULL && cJSON_IsString(reason)) {
        md_stop_reason_t stop_reason = md_stop_reason_from_string(reason->valuestring);
        md_state_set_stop_reason(model, stop_reason, NULL);
    }

    /* Parse description */
    cJSON *description = cJSON_GetObjectItemCaseSensitive(body, "description");
    if (description != NULL && cJSON_IsString(description)) {
        md_state_set_stop_reason(model, model->stop_reason, description->valuestring);
    }

    /* Parse thread ID */
    cJSON *thread_id = cJSON_GetObjectItemCaseSensitive(body, "threadId");
    if (thread_id != NULL && cJSON_IsNumber(thread_id)) {
        md_state_set_current_thread_id(model, thread_id->valueint);
    }

    /* Parse hit breakpoint IDs */
    cJSON *hit_bp = cJSON_GetObjectItemCaseSensitive(body, "hitBreakpointIds");
    if (hit_bp != NULL && cJSON_IsArray(hit_bp)) {
        int num_ids = cJSON_GetArraySize(hit_bp);
        for (int i = 0; i < num_ids; i++) {
            cJSON *id = cJSON_GetArrayItem(hit_bp, i);
            if (id != NULL && cJSON_IsNumber(id)) {
                /* Update breakpoint hit count */
                md_breakpoint_t bp;
                if (md_state_find_breakpoint_by_dap_id(model, id->valueint, &bp) == MD_SUCCESS) {
                    bp.hit_count++;
                    md_state_update_breakpoint(model, &bp);
                    model->debug_state.total_breakpoint_hits++;
                }
            }
        }
    }

    /* Set state to stopped */
    md_state_set_exec_state(model, MD_EXEC_STATE_STOPPED);

    MD_INFO("Program stopped: %s", model->stop_description);
    return MD_SUCCESS;
}

md_error_t md_state_handle_continued_event(md_state_model_t *model,
                                           const cJSON *body) {
    if (model == NULL || body == NULL) return MD_ERROR_INVALID_ARG;

    (void)body;  /* body contains threadId, allThreadsContinued */

    md_state_set_exec_state(model, MD_EXEC_STATE_RUNNING);
    return MD_SUCCESS;
}

md_error_t md_state_handle_exited_event(md_state_model_t *model,
                                        const cJSON *body) {
    if (model == NULL || body == NULL) return MD_ERROR_INVALID_ARG;

    cJSON *exit_code = cJSON_GetObjectItemCaseSensitive(body, "exitCode");
    if (exit_code != NULL && cJSON_IsNumber(exit_code)) {
        model->debug_state.exit_code = exit_code->valueint;
    }

    md_state_set_exec_state(model, MD_EXEC_STATE_TERMINATED);
    MD_INFO("Program exited with code %d", model->debug_state.exit_code);
    return MD_SUCCESS;
}

md_error_t md_state_handle_terminated_event(md_state_model_t *model,
                                            const cJSON *body) {
    if (model == NULL) return MD_ERROR_INVALID_ARG;

    (void)body;  /* body may contain restart */

    md_state_set_exec_state(model, MD_EXEC_STATE_TERMINATED);
    MD_INFO("Debug session terminated");
    return MD_SUCCESS;
}

md_error_t md_state_handle_thread_event(md_state_model_t *model,
                                        const cJSON *body) {
    if (model == NULL || body == NULL) return MD_ERROR_INVALID_ARG;

    cJSON *reason = cJSON_GetObjectItemCaseSensitive(body, "reason");
    cJSON *thread_id = cJSON_GetObjectItemCaseSensitive(body, "threadId");

    if (thread_id == NULL || !cJSON_IsNumber(thread_id)) {
        return MD_ERROR_INVALID_ARG;
    }

    int tid = thread_id->valueint;
    const char *reason_str = reason != NULL && cJSON_IsString(reason) ?
                             reason->valuestring : "unknown";

    if (strcmp(reason_str, "started") == 0) {
        md_thread_t thread;
        md_thread_init(&thread);
        thread.id = tid;
        thread.state = MD_THREAD_STATE_RUNNING;
        md_state_add_thread(model, &thread);
    } else if (strcmp(reason_str, "exited") == 0) {
        md_state_remove_thread(model, tid);
    }

    return MD_SUCCESS;
}

md_error_t md_state_handle_breakpoint_event(md_state_model_t *model,
                                            const cJSON *body) {
    if (model == NULL || body == NULL) return MD_ERROR_INVALID_ARG;

    cJSON *reason = cJSON_GetObjectItemCaseSensitive(body, "reason");
    cJSON *bp_json = cJSON_GetObjectItemCaseSensitive(body, "breakpoint");

    if (bp_json == NULL) return MD_ERROR_INVALID_ARG;

    const char *reason_str = reason != NULL && cJSON_IsString(reason) ?
                             reason->valuestring : "changed";

    /* Parse breakpoint */
    md_breakpoint_t bp;
    md_breakpoint_init(&bp);

    cJSON *id = cJSON_GetObjectItemCaseSensitive(bp_json, "id");
    cJSON *verified = cJSON_GetObjectItemCaseSensitive(bp_json, "verified");
    cJSON *line = cJSON_GetObjectItemCaseSensitive(bp_json, "line");
    cJSON *source = cJSON_GetObjectItemCaseSensitive(bp_json, "source");
    cJSON *message = cJSON_GetObjectItemCaseSensitive(bp_json, "message");

    if (id != NULL && cJSON_IsNumber(id)) bp.dap_id = id->valueint;
    if (verified != NULL && cJSON_IsBool(verified)) bp.verified = cJSON_IsTrue(verified);
    if (line != NULL && cJSON_IsNumber(line)) bp.line = line->valueint;
    if (message != NULL && cJSON_IsString(message)) {
        safe_strncpy(bp.message, message->valuestring, sizeof(bp.message));
    }

    if (source != NULL) {
        cJSON *path = cJSON_GetObjectItemCaseSensitive(source, "path");
        if (path != NULL && cJSON_IsString(path)) {
            safe_strncpy(bp.path, path->valuestring, sizeof(bp.path));
        }
    }

    bp.state = bp.verified ? MD_BP_STATE_VERIFIED : MD_BP_STATE_PENDING;

    /* Find existing or add new */
    md_breakpoint_t existing;
    if (md_state_find_breakpoint_by_dap_id(model, bp.dap_id, &existing) == MD_SUCCESS) {
        bp.id = existing.id;
        md_state_update_breakpoint(model, &bp);
    } else if (strcmp(reason_str, "new") == 0 || strcmp(reason_str, "changed") == 0) {
        md_state_add_breakpoint(model, &bp);
    }

    return MD_SUCCESS;
}

md_error_t md_state_handle_module_event(md_state_model_t *model,
                                        const cJSON *body) {
    if (model == NULL || body == NULL) return MD_ERROR_INVALID_ARG;

    cJSON *reason = cJSON_GetObjectItemCaseSensitive(body, "reason");
    cJSON *module_json = cJSON_GetObjectItemCaseSensitive(body, "module");

    if (module_json == NULL) return MD_ERROR_INVALID_ARG;

    const char *reason_str = reason != NULL && cJSON_IsString(reason) ?
                             reason->valuestring : "new";

    md_module_t module;
    md_module_init(&module);

    cJSON *id = cJSON_GetObjectItemCaseSensitive(module_json, "id");
    cJSON *name = cJSON_GetObjectItemCaseSensitive(module_json, "name");
    cJSON *path = cJSON_GetObjectItemCaseSensitive(module_json, "path");
    cJSON *symbol_status = cJSON_GetObjectItemCaseSensitive(module_json, "symbolStatus");

    if (id != NULL && cJSON_IsNumber(id)) module.id = id->valueint;
    if (name != NULL && cJSON_IsString(name)) {
        safe_strncpy(module.name, name->valuestring, sizeof(module.name));
    }
    if (path != NULL && cJSON_IsString(path)) {
        safe_strncpy(module.path, path->valuestring, sizeof(module.path));
    }
    if (symbol_status != NULL && cJSON_IsString(symbol_status)) {
        safe_strncpy(module.symbol_status, symbol_status->valuestring, sizeof(module.symbol_status));
        module.symbols_loaded = strcmp(symbol_status->valuestring, "Symbols loaded.") == 0;
    }

    module.state = MD_MODULE_STATE_LOADED;

    if (strcmp(reason_str, "new") == 0 || strcmp(reason_str, "changed") == 0) {
        md_state_add_module(model, &module);
    } else if (strcmp(reason_str, "removed") == 0) {
        md_state_remove_module(model, module.id);
    }

    return MD_SUCCESS;
}

md_error_t md_state_handle_output_event(md_state_model_t *model,
                                        const cJSON *body,
                                        void (*callback)(const char *category,
                                                         const char *output,
                                                         void *user_data),
                                        void *user_data) {
    if (model == NULL || body == NULL || callback == NULL) {
        return MD_ERROR_INVALID_ARG;
    }

    cJSON *category = cJSON_GetObjectItemCaseSensitive(body, "category");
    cJSON *output = cJSON_GetObjectItemCaseSensitive(body, "output");

    const char *category_str = category != NULL && cJSON_IsString(category) ?
                               category->valuestring : "console";
    const char *output_str = output != NULL && cJSON_IsString(output) ?
                             output->valuestring : "";

    callback(category_str, output_str, user_data);
    return MD_SUCCESS;
}

/* ============================================================================
 * DAP Response Parsing
 * ============================================================================ */

md_error_t md_state_parse_threads_response(md_state_model_t *model,
                                           const cJSON *body) {
    if (model == NULL || body == NULL) return MD_ERROR_INVALID_ARG;

    cJSON *threads = cJSON_GetObjectItemCaseSensitive(body, "threads");
    if (threads == NULL || !cJSON_IsArray(threads)) {
        return MD_ERROR_INVALID_ARG;
    }

    int num_threads = cJSON_GetArraySize(threads);
    for (int i = 0; i < num_threads; i++) {
        cJSON *thr = cJSON_GetArrayItem(threads, i);
        if (thr == NULL) continue;

        md_thread_t thread;
        md_thread_init(&thread);

        cJSON *id = cJSON_GetObjectItemCaseSensitive(thr, "id");
        cJSON *name = cJSON_GetObjectItemCaseSensitive(thr, "name");

        if (id != NULL && cJSON_IsNumber(id)) thread.id = id->valueint;
        if (name != NULL && cJSON_IsString(name)) {
            safe_strncpy(thread.name, name->valuestring, sizeof(thread.name));
        }

        thread.state = MD_THREAD_STATE_STOPPED;
        thread.is_stopped = true;
        thread.is_main = (i == 0);

        md_state_add_thread(model, &thread);
    }

    MD_DEBUG("Parsed %d threads", num_threads);
    return MD_SUCCESS;
}

md_error_t md_state_parse_stacktrace_response(md_state_model_t *model,
                                              int thread_id,
                                              const cJSON *body) {
    if (model == NULL || body == NULL) return MD_ERROR_INVALID_ARG;

    cJSON *stack_frames = cJSON_GetObjectItemCaseSensitive(body, "stackFrames");
    if (stack_frames == NULL || !cJSON_IsArray(stack_frames)) {
        return MD_ERROR_INVALID_ARG;
    }

    int num_frames = cJSON_GetArraySize(stack_frames);
    if (num_frames == 0) {
        return MD_SUCCESS;
    }

    md_stack_frame_t *frames = (md_stack_frame_t*)calloc(num_frames, sizeof(md_stack_frame_t));
    if (frames == NULL) return MD_ERROR_OUT_OF_MEMORY;

    for (int i = 0; i < num_frames; i++) {
        cJSON *frame = cJSON_GetArrayItem(stack_frames, i);
        if (frame == NULL) continue;

        md_stack_frame_init(&frames[i]);

        cJSON *id = cJSON_GetObjectItemCaseSensitive(frame, "id");
        cJSON *name = cJSON_GetObjectItemCaseSensitive(frame, "name");
        cJSON *line = cJSON_GetObjectItemCaseSensitive(frame, "line");
        cJSON *column = cJSON_GetObjectItemCaseSensitive(frame, "column");
        cJSON *source = cJSON_GetObjectItemCaseSensitive(frame, "source");
        cJSON *module_id = cJSON_GetObjectItemCaseSensitive(frame, "moduleId");
        cJSON *presentation = cJSON_GetObjectItemCaseSensitive(frame, "presentationHint");
        cJSON *instr_ptr = cJSON_GetObjectItemCaseSensitive(frame, "instructionPointerReference");

        if (id != NULL && cJSON_IsNumber(id)) frames[i].id = id->valueint;
        if (name != NULL && cJSON_IsString(name)) {
            safe_strncpy(frames[i].name, name->valuestring, sizeof(frames[i].name));
        }
        if (line != NULL && cJSON_IsNumber(line)) frames[i].line = line->valueint;
        if (column != NULL && cJSON_IsNumber(column)) frames[i].column = column->valueint;
        if (module_id != NULL && cJSON_IsNumber(module_id)) frames[i].thread_id = module_id->valueint;
        if (presentation != NULL && cJSON_IsString(presentation)) {
            safe_strncpy(frames[i].presentation_hint, presentation->valuestring,
                         sizeof(frames[i].presentation_hint));
        }
        if (instr_ptr != NULL && cJSON_IsString(instr_ptr)) {
            frames[i].instruction_pointer = strtoull(instr_ptr->valuestring, NULL, 16);
        }

        if (source != NULL) {
            cJSON *path = cJSON_GetObjectItemCaseSensitive(source, "path");
            cJSON *src_name = cJSON_GetObjectItemCaseSensitive(source, "name");
            if (path != NULL && cJSON_IsString(path)) {
                safe_strncpy(frames[i].source_path, path->valuestring, sizeof(frames[i].source_path));
            }
            if (src_name != NULL && cJSON_IsString(src_name)) {
                /* source name is usually same as path basename */
            }
        }

        frames[i].thread_id = thread_id;
    }

    md_error_t err = md_state_set_stack_frames(model, thread_id, frames, num_frames);

    /* Set current frame if not set */
    if (err == MD_SUCCESS && model->current_frame_id == MD_INVALID_ID && num_frames > 0) {
        md_state_set_current_frame_id(model, frames[0].id);
    }

    free(frames);
    MD_DEBUG("Parsed %d stack frames for thread %d", num_frames, thread_id);
    return err;
}

md_error_t md_state_parse_scopes_response(md_state_model_t *model,
                                          int frame_id,
                                          const cJSON *body) {
    if (model == NULL || body == NULL) return MD_ERROR_INVALID_ARG;

    cJSON *scopes = cJSON_GetObjectItemCaseSensitive(body, "scopes");
    if (scopes == NULL || !cJSON_IsArray(scopes)) {
        return MD_ERROR_INVALID_ARG;
    }

    int num_scopes = cJSON_GetArraySize(scopes);
    if (num_scopes == 0) {
        return MD_SUCCESS;
    }

    md_scope_t *scope_arr = (md_scope_t*)calloc(num_scopes, sizeof(md_scope_t));
    if (scope_arr == NULL) return MD_ERROR_OUT_OF_MEMORY;

    for (int i = 0; i < num_scopes; i++) {
        cJSON *scope = cJSON_GetArrayItem(scopes, i);
        if (scope == NULL) continue;

        md_scope_init(&scope_arr[i]);

        cJSON *name = cJSON_GetObjectItemCaseSensitive(scope, "name");
        cJSON *var_ref = cJSON_GetObjectItemCaseSensitive(scope, "variablesReference");
        cJSON *named_vars = cJSON_GetObjectItemCaseSensitive(scope, "namedVariables");
        cJSON *indexed_vars = cJSON_GetObjectItemCaseSensitive(scope, "indexedVariables");
        cJSON *expensive = cJSON_GetObjectItemCaseSensitive(scope, "expensive");
        cJSON *hint = cJSON_GetObjectItemCaseSensitive(scope, "presentationHint");

        if (name != NULL && cJSON_IsString(name)) {
            safe_strncpy(scope_arr[i].name, name->valuestring, sizeof(scope_arr[i].name));
        }
        if (var_ref != NULL && cJSON_IsNumber(var_ref)) {
            scope_arr[i].id = var_ref->valueint;
        }
        if (named_vars != NULL && cJSON_IsNumber(named_vars)) {
            scope_arr[i].named_variables = named_vars->valueint;
        }
        if (indexed_vars != NULL && cJSON_IsNumber(indexed_vars)) {
            scope_arr[i].indexed_variables = indexed_vars->valueint;
        }
        if (expensive != NULL && cJSON_IsBool(expensive)) {
            scope_arr[i].expensive = cJSON_IsTrue(expensive);
        }
        if (hint != NULL && cJSON_IsString(hint)) {
            if (strcmp(hint->valuestring, "arguments") == 0) {
                scope_arr[i].type = MD_SCOPE_TYPE_ARGUMENTS;
            } else if (strcmp(hint->valuestring, "locals") == 0) {
                scope_arr[i].type = MD_SCOPE_TYPE_LOCALS;
            } else if (strcmp(hint->valuestring, "registers") == 0) {
                scope_arr[i].type = MD_SCOPE_TYPE_REGISTERS;
            } else if (strcmp(hint->valuestring, "globals") == 0) {
                scope_arr[i].type = MD_SCOPE_TYPE_GLOBALS;
            }
        }

        scope_arr[i].frame_id = frame_id;
    }

    md_error_t err = md_state_set_scopes(model, frame_id, scope_arr, num_scopes);
    free(scope_arr);

    MD_DEBUG("Parsed %d scopes for frame %d", num_scopes, frame_id);
    return err;
}

md_error_t md_state_parse_variables_response(md_state_model_t *model,
                                             int scope_id,
                                             const cJSON *body) {
    if (model == NULL || body == NULL) return MD_ERROR_INVALID_ARG;

    cJSON *variables = cJSON_GetObjectItemCaseSensitive(body, "variables");
    if (variables == NULL || !cJSON_IsArray(variables)) {
        return MD_ERROR_INVALID_ARG;
    }

    int num_vars = cJSON_GetArraySize(variables);
    if (num_vars == 0) {
        return MD_SUCCESS;
    }

    md_variable_t *var_arr = (md_variable_t*)calloc(num_vars, sizeof(md_variable_t));
    if (var_arr == NULL) return MD_ERROR_OUT_OF_MEMORY;

    for (int i = 0; i < num_vars; i++) {
        cJSON *var = cJSON_GetArrayItem(variables, i);
        if (var == NULL) continue;

        md_variable_init(&var_arr[i]);

        cJSON *name = cJSON_GetObjectItemCaseSensitive(var, "name");
        cJSON *value = cJSON_GetObjectItemCaseSensitive(var, "value");
        cJSON *type = cJSON_GetObjectItemCaseSensitive(var, "type");
        cJSON *var_ref = cJSON_GetObjectItemCaseSensitive(var, "variablesReference");
        cJSON *named_vars = cJSON_GetObjectItemCaseSensitive(var, "namedVariables");
        cJSON *indexed_vars = cJSON_GetObjectItemCaseSensitive(var, "indexedVariables");
        cJSON *eval_name = cJSON_GetObjectItemCaseSensitive(var, "evaluateName");
        cJSON *hint = cJSON_GetObjectItemCaseSensitive(var, "presentationHint");
        cJSON *mem_ref = cJSON_GetObjectItemCaseSensitive(var, "memoryReference");

        if (name != NULL && cJSON_IsString(name)) {
            safe_strncpy(var_arr[i].name, name->valuestring, sizeof(var_arr[i].name));
        }
        if (value != NULL && cJSON_IsString(value)) {
            safe_strncpy(var_arr[i].value, value->valuestring, sizeof(var_arr[i].value));
        }
        if (type != NULL && cJSON_IsString(type)) {
            safe_strncpy(var_arr[i].type, type->valuestring, sizeof(var_arr[i].type));
        }
        if (var_ref != NULL && cJSON_IsNumber(var_ref)) {
            var_arr[i].variables_reference = var_ref->valueint;
        }
        if (named_vars != NULL && cJSON_IsNumber(named_vars)) {
            var_arr[i].named_children = named_vars->valueint;
        }
        if (indexed_vars != NULL && cJSON_IsNumber(indexed_vars)) {
            var_arr[i].indexed_children = indexed_vars->valueint;
        }
        if (eval_name != NULL && cJSON_IsString(eval_name)) {
            safe_strncpy(var_arr[i].evaluate_name, eval_name->valuestring,
                         sizeof(var_arr[i].evaluate_name));
        }
        if (mem_ref != NULL && cJSON_IsString(mem_ref)) {
            var_arr[i].memory_reference = strtoull(mem_ref->valuestring, NULL, 16);
        }
        if (hint != NULL && cJSON_IsString(hint)) {
            safe_strncpy(var_arr[i].presentation_hint, hint->valuestring,
                         sizeof(var_arr[i].presentation_hint));
        }

        var_arr[i].id = scope_id * 1000 + i;  /* Generate unique ID */
        var_arr[i].scope_id = scope_id;
    }

    md_error_t err = md_state_set_variables(model, scope_id, var_arr, num_vars);
    free(var_arr);

    MD_DEBUG("Parsed %d variables for scope %d", num_vars, scope_id);
    return err;
}

md_error_t md_state_parse_set_breakpoints_response(md_state_model_t *model,
                                                   const char *path,
                                                   const dap_source_breakpoint_t *breakpoints,
                                                   int bp_count,
                                                   const cJSON *body) {
    if (model == NULL || body == NULL) return MD_ERROR_INVALID_ARG;

    cJSON *bps = cJSON_GetObjectItemCaseSensitive(body, "breakpoints");
    if (bps == NULL || !cJSON_IsArray(bps)) {
        return MD_ERROR_INVALID_ARG;
    }

    int num_bps = cJSON_GetArraySize(bps);

    for (int i = 0; i < num_bps && i < bp_count; i++) {
        cJSON *bp = cJSON_GetArrayItem(bps, i);
        if (bp == NULL) continue;

        md_breakpoint_t mdbp;
        md_breakpoint_init(&mdbp);

        cJSON *id = cJSON_GetObjectItemCaseSensitive(bp, "id");
        cJSON *verified = cJSON_GetObjectItemCaseSensitive(bp, "verified");
        cJSON *line = cJSON_GetObjectItemCaseSensitive(bp, "line");
        cJSON *message = cJSON_GetObjectItemCaseSensitive(bp, "message");

        if (id != NULL && cJSON_IsNumber(id)) mdbp.dap_id = id->valueint;
        if (verified != NULL && cJSON_IsBool(verified)) {
            mdbp.verified = cJSON_IsTrue(verified);
            mdbp.state = mdbp.verified ? MD_BP_STATE_VERIFIED : MD_BP_STATE_PENDING;
        }
        if (line != NULL && cJSON_IsNumber(line)) mdbp.line = line->valueint;
        if (message != NULL && cJSON_IsString(message)) {
            safe_strncpy(mdbp.message, message->valuestring, sizeof(mdbp.message));
        }

        /* Copy from request */
        safe_strncpy(mdbp.path, path, sizeof(mdbp.path));
        if (breakpoints[i].line > 0) mdbp.line = breakpoints[i].line;
        if (breakpoints[i].condition != NULL) {
            safe_strncpy(mdbp.condition, breakpoints[i].condition, sizeof(mdbp.condition));
            mdbp.type = MD_BP_TYPE_CONDITIONAL;
        }
        if (breakpoints[i].log_message != NULL) {
            safe_strncpy(mdbp.log_message, breakpoints[i].log_message, sizeof(mdbp.log_message));
            mdbp.type = MD_BP_TYPE_LOGPOINT;
        }
        mdbp.enabled = true;

        /* Check if breakpoint exists by location */
        md_breakpoint_t existing;
        if (md_state_find_breakpoint_by_location(model, path, breakpoints[i].line, &existing) == MD_SUCCESS) {
            mdbp.id = existing.id;
            md_state_update_breakpoint(model, &mdbp);
        } else {
            md_state_add_breakpoint(model, &mdbp);
        }
    }

    MD_DEBUG("Parsed %d breakpoints for %s", num_bps, path);
    return MD_SUCCESS;
}

md_error_t md_state_parse_initialize_response(md_state_model_t *model,
                                              const cJSON *body) {
    if (model == NULL || body == NULL) return MD_ERROR_INVALID_ARG;

    /* Free old capabilities if any */
    if (model->capabilities_set) {
        dap_capabilities_free(&model->capabilities);
    }

    dap_capabilities_init(&model->capabilities);

    cJSON *item;
#define PARSE_BOOL(name) \
    item = cJSON_GetObjectItemCaseSensitive(body, #name); \
    if (item && cJSON_IsBool(item)) model->capabilities.name = cJSON_IsTrue(item)

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

    model->capabilities_set = true;
    MD_INFO("Parsed debugger capabilities");
    return MD_SUCCESS;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

md_error_t md_state_dump(md_state_model_t *model, char *buffer, size_t size) {
    if (model == NULL || buffer == NULL || size == 0) {
        return MD_ERROR_INVALID_ARG;
    }

    int offset = 0;
    int ret;

    ret = snprintf(buffer + offset, size - offset,
                   "=== State Model Dump ===\n"
                   "Exec State: %s\n"
                   "Stop Reason: %s\n"
                   "Stop Description: %s\n"
                   "Current Thread: %d\n"
                   "Current Frame: %d\n"
                   "Breakpoints: %d\n"
                   "Threads: %d\n"
                   "Modules: %d\n"
                   "Has Exception: %s\n",
                   md_exec_state_string(model->exec_state),
                   md_stop_reason_string(model->stop_reason),
                   model->stop_description,
                   model->current_thread_id,
                   model->current_frame_id,
                   model->bp_count,
                   model->thread_count,
                   model->module_count,
                   model->has_exception ? "yes" : "no");

    if (ret < 0 || (size_t)ret >= size - offset) {
        return MD_ERROR_BUFFER_TOO_SMALL;
    }
    offset += ret;

    /* Dump breakpoints */
    if (model->bp_count > 0 && (size_t)offset < size) {
        ret = snprintf(buffer + offset, size - offset, "\n--- Breakpoints ---\n");
        if (ret > 0) offset += ret;

        md_bp_entry_t *entry = model->breakpoints;
        while (entry != NULL && (size_t)offset < size) {
            ret = snprintf(buffer + offset, size - offset,
                          "  [%d] %s:%d (%s, %s)\n",
                          entry->bp.id, entry->bp.path, entry->bp.line,
                          md_bp_type_string(entry->bp.type),
                          md_bp_state_string(entry->bp.state));
            if (ret > 0) offset += ret;
            entry = entry->next;
        }
    }

    return MD_SUCCESS;
}
