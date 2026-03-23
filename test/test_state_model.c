/**
 * @file test_state_model.c
 * @brief Test program for State Model (Phase 3)
 *
 * Tests:
 * 1. State model creation/destruction
 * 2. Execution state management
 * 3. Breakpoint management
 * 4. Thread management
 * 5. Stack frame management
 * 6. Variable/Scope management
 * 7. Module management
 * 8. Exception management
 * 9. DAP event handling
 * 10. DAP response parsing
 */

#include "types.h"
#include "logger.h"
#include "state_model.h"
#include "state_types.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ============================================================================
 * Test Macros
 * ============================================================================ */

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  [TEST] %s... ", #name); \
    test_##name(); \
    printf("PASSED\n"); \
} while (0)

#define ASSERT_TRUE(cond) assert(cond)
#define ASSERT_FALSE(cond) assert(!(cond))
#define ASSERT_EQ(a, b) assert((a) == (b))
#define ASSERT_NE(a, b) assert((a) != (b))
#define ASSERT_NOT_NULL(ptr) assert((ptr) != NULL)
#define ASSERT_NULL(ptr) assert((ptr) == NULL)
#define ASSERT_SUCCESS(err) assert((err) == MD_SUCCESS)
#define ASSERT_STR_EQ(a, b) assert(strcmp((a), (b)) == 0)

/* ============================================================================
 * Test: State Model Lifecycle
 * ============================================================================ */

TEST(state_model_create_destroy) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    /* Check initial state */
    ASSERT_EQ(md_state_get_exec_state(model), MD_EXEC_STATE_NONE);
    ASSERT_EQ(md_state_get_stop_reason(model), MD_STOP_REASON_UNKNOWN);
    ASSERT_EQ(md_state_get_current_thread_id(model), MD_INVALID_ID);
    ASSERT_EQ(md_state_get_current_frame_id(model), MD_INVALID_ID);
    ASSERT_EQ(md_state_get_breakpoint_count(model), 0);
    ASSERT_EQ(md_state_get_thread_count(model), 0);
    ASSERT_EQ(md_state_get_module_count(model), 0);
    ASSERT_FALSE(md_state_has_exception(model));

    md_state_model_destroy(model);
}

TEST(state_model_config) {
    md_state_config_t config;
    md_state_config_init(&config);

    config.max_breakpoints = 100;
    config.max_threads = 50;
    config.max_stack_frames = 64;
    config.auto_update_threads = true;
    config.auto_update_stack = true;

    md_state_model_t *model = md_state_model_create(&config);
    ASSERT_NOT_NULL(model);

    md_state_model_destroy(model);
}

TEST(state_model_reset) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    /* Add some state */
    md_state_set_exec_state(model, MD_EXEC_STATE_RUNNING);

    md_breakpoint_t bp;
    md_breakpoint_init(&bp);
    strcpy(bp.path, "/tmp/test.c");
    bp.line = 10;
    md_state_add_breakpoint(model, &bp);

    md_thread_t thread;
    md_thread_init(&thread);
    thread.id = 1;
    thread.state = MD_THREAD_STATE_RUNNING;
    md_state_add_thread(model, &thread);

    /* Reset */
    ASSERT_SUCCESS(md_state_model_reset(model));

    /* Verify reset state */
    ASSERT_EQ(md_state_get_exec_state(model), MD_EXEC_STATE_NONE);
    ASSERT_EQ(md_state_get_breakpoint_count(model), 0);
    ASSERT_EQ(md_state_get_thread_count(model), 0);

    md_state_model_destroy(model);
}

/* ============================================================================
 * Test: Execution State Management
 * ============================================================================ */

TEST(exec_state_management) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    /* Test state transitions */
    ASSERT_SUCCESS(md_state_set_exec_state(model, MD_EXEC_STATE_LAUNCHING));
    ASSERT_EQ(md_state_get_exec_state(model), MD_EXEC_STATE_LAUNCHING);

    ASSERT_SUCCESS(md_state_set_exec_state(model, MD_EXEC_STATE_RUNNING));
    ASSERT_EQ(md_state_get_exec_state(model), MD_EXEC_STATE_RUNNING);

    ASSERT_SUCCESS(md_state_set_exec_state(model, MD_EXEC_STATE_STOPPED));
    ASSERT_EQ(md_state_get_exec_state(model), MD_EXEC_STATE_STOPPED);

    /* Test state predicates */
    ASSERT_TRUE(md_exec_state_is_stopped(MD_EXEC_STATE_STOPPED));
    ASSERT_FALSE(md_exec_state_is_stopped(MD_EXEC_STATE_RUNNING));
    ASSERT_TRUE(md_exec_state_is_running(MD_EXEC_STATE_RUNNING));
    ASSERT_FALSE(md_exec_state_is_running(MD_EXEC_STATE_STOPPED));

    /* Test state strings */
    ASSERT_STR_EQ(md_exec_state_string(MD_EXEC_STATE_RUNNING), "running");
    ASSERT_STR_EQ(md_exec_state_string(MD_EXEC_STATE_STOPPED), "stopped");
    ASSERT_STR_EQ(md_exec_state_string(MD_EXEC_STATE_TERMINATED), "terminated");

    md_state_model_destroy(model);
}

TEST(stop_reason_management) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    ASSERT_SUCCESS(md_state_set_stop_reason(model, MD_STOP_REASON_BREAKPOINT, "Breakpoint hit"));
    ASSERT_EQ(md_state_get_stop_reason(model), MD_STOP_REASON_BREAKPOINT);

    /* Test stop reason strings */
    ASSERT_STR_EQ(md_stop_reason_string(MD_STOP_REASON_STEP), "step");
    ASSERT_STR_EQ(md_stop_reason_string(MD_STOP_REASON_BREAKPOINT), "breakpoint");
    ASSERT_STR_EQ(md_stop_reason_string(MD_STOP_REASON_EXCEPTION), "exception");

    /* Test stop reason parsing */
    ASSERT_EQ(md_stop_reason_from_string("step"), MD_STOP_REASON_STEP);
    ASSERT_EQ(md_stop_reason_from_string("breakpoint"), MD_STOP_REASON_BREAKPOINT);
    ASSERT_EQ(md_stop_reason_from_string("unknown"), MD_STOP_REASON_UNKNOWN);
    ASSERT_EQ(md_stop_reason_from_string(NULL), MD_STOP_REASON_UNKNOWN);

    md_state_model_destroy(model);
}

TEST(thread_frame_selection) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    ASSERT_SUCCESS(md_state_set_current_thread_id(model, 1));
    ASSERT_EQ(md_state_get_current_thread_id(model), 1);

    ASSERT_SUCCESS(md_state_set_current_frame_id(model, 2));
    ASSERT_EQ(md_state_get_current_frame_id(model), 2);

    md_state_model_destroy(model);
}

/* ============================================================================
 * Test: Breakpoint Management
 * ============================================================================ */

TEST(breakpoint_add_remove) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    /* Add breakpoint */
    md_breakpoint_t bp;
    md_breakpoint_init(&bp);
    strcpy(bp.path, "/tmp/test.c");
    bp.line = 42;
    bp.type = MD_BP_TYPE_LINE;

    int id = md_state_add_breakpoint(model, &bp);
    ASSERT_TRUE(id > 0);
    ASSERT_EQ(md_state_get_breakpoint_count(model), 1);

    /* Get breakpoint */
    md_breakpoint_t retrieved;
    ASSERT_SUCCESS(md_state_get_breakpoint(model, id, &retrieved));
    ASSERT_EQ(retrieved.id, id);
    ASSERT_EQ(retrieved.line, 42);
    ASSERT_STR_EQ(retrieved.path, "/tmp/test.c");

    /* Find by location */
    md_breakpoint_t found;
    ASSERT_SUCCESS(md_state_find_breakpoint_by_location(model, "/tmp/test.c", 42, &found));
    ASSERT_EQ(found.id, id);

    /* Remove breakpoint */
    ASSERT_SUCCESS(md_state_remove_breakpoint(model, id));
    ASSERT_EQ(md_state_get_breakpoint_count(model), 0);

    /* Should not find anymore */
    ASSERT_NE(md_state_get_breakpoint(model, id, &retrieved), MD_SUCCESS);

    md_state_model_destroy(model);
}

TEST(breakpoint_update) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    md_breakpoint_t bp;
    md_breakpoint_init(&bp);
    strcpy(bp.path, "/tmp/test.c");
    bp.line = 10;

    int id = md_state_add_breakpoint(model, &bp);

    /* Update breakpoint */
    bp.id = id;
    bp.line = 20;
    bp.dap_id = 100;
    bp.verified = true;
    bp.state = MD_BP_STATE_VERIFIED;

    ASSERT_SUCCESS(md_state_update_breakpoint(model, &bp));

    /* Verify update */
    md_breakpoint_t retrieved;
    ASSERT_SUCCESS(md_state_get_breakpoint(model, id, &retrieved));
    ASSERT_EQ(retrieved.line, 20);
    ASSERT_EQ(retrieved.dap_id, 100);
    ASSERT_TRUE(retrieved.verified);

    md_state_model_destroy(model);
}

TEST(breakpoint_enable_disable) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    md_breakpoint_t bp;
    md_breakpoint_init(&bp);
    strcpy(bp.path, "/tmp/test.c");
    bp.line = 10;
    bp.enabled = true;

    int id = md_state_add_breakpoint(model, &bp);

    /* Disable */
    ASSERT_SUCCESS(md_state_set_breakpoint_enabled(model, id, false));

    md_breakpoint_t retrieved;
    ASSERT_SUCCESS(md_state_get_breakpoint(model, id, &retrieved));
    ASSERT_FALSE(retrieved.enabled);

    /* Enable */
    ASSERT_SUCCESS(md_state_set_breakpoint_enabled(model, id, true));
    ASSERT_SUCCESS(md_state_get_breakpoint(model, id, &retrieved));
    ASSERT_TRUE(retrieved.enabled);

    md_state_model_destroy(model);
}

TEST(breakpoint_get_all) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    /* Add multiple breakpoints */
    for (int i = 0; i < 5; i++) {
        md_breakpoint_t bp;
        md_breakpoint_init(&bp);
        snprintf(bp.path, sizeof(bp.path), "/tmp/test%d.c", i);
        bp.line = 10 + i;
        md_state_add_breakpoint(model, &bp);
    }

    ASSERT_EQ(md_state_get_breakpoint_count(model), 5);

    /* Get all */
    md_breakpoint_t bps[10];
    int count;
    ASSERT_SUCCESS(md_state_get_all_breakpoints(model, bps, 10, &count));
    ASSERT_EQ(count, 5);

    /* Clear all */
    ASSERT_SUCCESS(md_state_clear_all_breakpoints(model));
    ASSERT_EQ(md_state_get_breakpoint_count(model), 0);

    md_state_model_destroy(model);
}

/* ============================================================================
 * Test: Thread Management
 * ============================================================================ */

TEST(thread_add_remove) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    /* Add thread */
    md_thread_t thread;
    md_thread_init(&thread);
    thread.id = 123;
    strcpy(thread.name, "main thread");
    thread.state = MD_THREAD_STATE_STOPPED;
    thread.is_main = true;

    ASSERT_SUCCESS(md_state_add_thread(model, &thread));
    ASSERT_EQ(md_state_get_thread_count(model), 1);

    /* Get thread */
    md_thread_t retrieved;
    ASSERT_SUCCESS(md_state_get_thread(model, 123, &retrieved));
    ASSERT_EQ(retrieved.id, 123);
    ASSERT_STR_EQ(retrieved.name, "main thread");
    ASSERT_TRUE(retrieved.is_main);

    /* Should set as current thread */
    ASSERT_EQ(md_state_get_current_thread_id(model), 123);

    /* Remove thread */
    ASSERT_SUCCESS(md_state_remove_thread(model, 123));
    ASSERT_EQ(md_state_get_thread_count(model), 0);

    md_state_model_destroy(model);
}

TEST(thread_update) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    md_thread_t thread;
    md_thread_init(&thread);
    thread.id = 1;
    thread.state = MD_THREAD_STATE_RUNNING;
    md_state_add_thread(model, &thread);

    /* Update */
    thread.state = MD_THREAD_STATE_STOPPED;
    thread.is_stopped = true;
    ASSERT_SUCCESS(md_state_update_thread(model, &thread));

    md_thread_t retrieved;
    ASSERT_SUCCESS(md_state_get_thread(model, 1, &retrieved));
    ASSERT_EQ(retrieved.state, MD_THREAD_STATE_STOPPED);
    ASSERT_TRUE(retrieved.is_stopped);

    md_state_model_destroy(model);
}

/* ============================================================================
 * Test: Stack Frame Management
 * ============================================================================ */

TEST(stack_frame_management) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    /* Create frames */
    md_stack_frame_t frames[3];
    for (int i = 0; i < 3; i++) {
        md_stack_frame_init(&frames[i]);
        frames[i].id = i + 1;
        frames[i].line = 10 + i;
        strcpy(frames[i].name, i == 0 ? "main" : "func");
        strcpy(frames[i].source_path, "/tmp/test.c");
        frames[i].thread_id = 1;
    }

    /* Set frames for thread 1 */
    ASSERT_SUCCESS(md_state_set_stack_frames(model, 1, frames, 3));
    ASSERT_EQ(md_state_get_stack_frame_count(model, 1), 3);

    /* Get frames */
    md_stack_frame_t retrieved[3];
    int count;
    ASSERT_SUCCESS(md_state_get_stack_frames(model, 1, retrieved, 3, &count));
    ASSERT_EQ(count, 3);
    ASSERT_EQ(retrieved[0].id, 1);

    /* Get single frame */
    md_stack_frame_t frame;
    ASSERT_SUCCESS(md_state_get_stack_frame(model, 1, &frame));
    ASSERT_STR_EQ(frame.name, "main");

    /* Clear frames */
    ASSERT_SUCCESS(md_state_clear_stack_frames(model, 1));
    ASSERT_EQ(md_state_get_stack_frame_count(model, 1), 0);

    md_state_model_destroy(model);
}

/* ============================================================================
 * Test: Variable/Scope Management
 * ============================================================================ */

TEST(scope_management) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    /* Create scopes */
    md_scope_t scopes[2];
    for (int i = 0; i < 2; i++) {
        md_scope_init(&scopes[i]);
        scopes[i].id = i + 1;
        scopes[i].frame_id = 1;
        scopes[i].type = i == 0 ? MD_SCOPE_TYPE_LOCALS : MD_SCOPE_TYPE_ARGUMENTS;
        scopes[i].named_variables = 5;
    }
    strcpy(scopes[0].name, "Locals");
    strcpy(scopes[1].name, "Arguments");

    ASSERT_SUCCESS(md_state_set_scopes(model, 1, scopes, 2));

    /* Get scopes */
    md_scope_t retrieved[2];
    int count;
    ASSERT_SUCCESS(md_state_get_scopes(model, 1, retrieved, 2, &count));
    ASSERT_EQ(count, 2);

    md_state_model_destroy(model);
}

TEST(variable_management) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    /* Create variables */
    md_variable_t vars[3];
    for (int i = 0; i < 3; i++) {
        md_variable_init(&vars[i]);
        vars[i].id = i + 1;
        snprintf(vars[i].name, sizeof(vars[i].name), "var%d", i);
        snprintf(vars[i].value, sizeof(vars[i].value), "%d", i * 10);
        vars[i].scope_id = 1;
    }
    strcpy(vars[0].type, "int");

    ASSERT_SUCCESS(md_state_set_variables(model, 1, vars, 3));

    /* Get variables */
    md_variable_t retrieved[3];
    int count;
    ASSERT_SUCCESS(md_state_get_variables(model, 1, retrieved, 3, &count));
    ASSERT_EQ(count, 3);
    ASSERT_STR_EQ(retrieved[0].name, "var0");
    ASSERT_STR_EQ(retrieved[0].value, "0");

    /* Get single variable */
    md_variable_t var;
    ASSERT_SUCCESS(md_state_get_variable(model, 1, &var));
    ASSERT_STR_EQ(var.name, "var0");

    md_state_model_destroy(model);
}

/* ============================================================================
 * Test: Module Management
 * ============================================================================ */

TEST(module_management) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    /* Add module */
    md_module_t module;
    md_module_init(&module);
    module.id = 1;
    strcpy(module.name, "test.exe");
    strcpy(module.path, "/tmp/test.exe");
    module.state = MD_MODULE_STATE_LOADED;
    module.symbols_loaded = true;

    ASSERT_SUCCESS(md_state_add_module(model, &module));
    ASSERT_EQ(md_state_get_module_count(model), 1);

    /* Get module */
    md_module_t retrieved;
    ASSERT_SUCCESS(md_state_get_module(model, 1, &retrieved));
    ASSERT_STR_EQ(retrieved.name, "test.exe");
    ASSERT_TRUE(retrieved.symbols_loaded);

    /* Update */
    module.state = MD_MODULE_STATE_UNLOADED;
    ASSERT_SUCCESS(md_state_update_module(model, &module));

    /* Remove */
    ASSERT_SUCCESS(md_state_remove_module(model, 1));
    ASSERT_EQ(md_state_get_module_count(model), 0);

    md_state_model_destroy(model);
}

/* ============================================================================
 * Test: Exception Management
 * ============================================================================ */

TEST(exception_management) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    ASSERT_FALSE(md_state_has_exception(model));

    /* Set exception */
    md_exception_t exc;
    md_exception_init(&exc);
    strcpy(exc.type, "SIGSEGV");
    strcpy(exc.message, "Segmentation fault");
    exc.thread_id = 1;
    exc.line = 42;

    ASSERT_SUCCESS(md_state_set_exception(model, &exc));
    ASSERT_TRUE(md_state_has_exception(model));

    /* Get exception */
    md_exception_t retrieved;
    ASSERT_SUCCESS(md_state_get_exception(model, &retrieved));
    ASSERT_STR_EQ(retrieved.type, "SIGSEGV");
    ASSERT_STR_EQ(retrieved.message, "Segmentation fault");

    /* Clear */
    ASSERT_SUCCESS(md_state_clear_exception(model));
    ASSERT_FALSE(md_state_has_exception(model));

    md_state_model_destroy(model);
}

/* ============================================================================
 * Test: DAP Event Handling
 * ============================================================================ */

TEST(dap_stopped_event) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    /* Create stopped event JSON */
    const char *json_str = "{"
        "\"reason\":\"breakpoint\","
        "\"description\":\"Breakpoint 1 hit\","
        "\"threadId\":1,"
        "\"hitBreakpointIds\":[1,2]"
    "}";

    cJSON *body = cJSON_Parse(json_str);
    ASSERT_NOT_NULL(body);

    ASSERT_SUCCESS(md_state_handle_stopped_event(model, body));

    ASSERT_EQ(md_state_get_exec_state(model), MD_EXEC_STATE_STOPPED);
    ASSERT_EQ(md_state_get_stop_reason(model), MD_STOP_REASON_BREAKPOINT);
    ASSERT_EQ(md_state_get_current_thread_id(model), 1);

    cJSON_Delete(body);
    md_state_model_destroy(model);
}

TEST(dap_continued_event) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    md_state_set_exec_state(model, MD_EXEC_STATE_STOPPED);

    const char *json_str = "{\"threadId\":1,\"allThreadsContinued\":true}";
    cJSON *body = cJSON_Parse(json_str);
    ASSERT_NOT_NULL(body);

    ASSERT_SUCCESS(md_state_handle_continued_event(model, body));
    ASSERT_EQ(md_state_get_exec_state(model), MD_EXEC_STATE_RUNNING);

    cJSON_Delete(body);
    md_state_model_destroy(model);
}

TEST(dap_exited_event) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    const char *json_str = "{\"exitCode\":0}";
    cJSON *body = cJSON_Parse(json_str);
    ASSERT_NOT_NULL(body);

    ASSERT_SUCCESS(md_state_handle_exited_event(model, body));
    ASSERT_EQ(md_state_get_exec_state(model), MD_EXEC_STATE_TERMINATED);

    md_debug_state_t state;
    ASSERT_SUCCESS(md_state_get_debug_state(model, &state));
    ASSERT_EQ(state.exit_code, 0);

    cJSON_Delete(body);
    md_state_model_destroy(model);
}

TEST(dap_thread_event) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    /* Thread started */
    const char *json_str = "{\"reason\":\"started\",\"threadId\":123}";
    cJSON *body = cJSON_Parse(json_str);
    ASSERT_NOT_NULL(body);

    ASSERT_SUCCESS(md_state_handle_thread_event(model, body));
    ASSERT_EQ(md_state_get_thread_count(model), 1);

    cJSON_Delete(body);

    /* Thread exited */
    json_str = "{\"reason\":\"exited\",\"threadId\":123}";
    body = cJSON_Parse(json_str);
    ASSERT_NOT_NULL(body);

    ASSERT_SUCCESS(md_state_handle_thread_event(model, body));
    ASSERT_EQ(md_state_get_thread_count(model), 0);

    cJSON_Delete(body);
    md_state_model_destroy(model);
}

/* ============================================================================
 * Test: DAP Response Parsing
 * ============================================================================ */

TEST(dap_threads_response) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    const char *json_str = "{"
        "\"threads\":["
            "{\"id\":1,\"name\":\"main thread\"},"
            "{\"id\":2,\"name\":\"worker\"}"
        "]"
    "}";

    cJSON *body = cJSON_Parse(json_str);
    ASSERT_NOT_NULL(body);

    ASSERT_SUCCESS(md_state_parse_threads_response(model, body));
    ASSERT_EQ(md_state_get_thread_count(model), 2);

    md_thread_t thread;
    ASSERT_SUCCESS(md_state_get_thread(model, 1, &thread));
    ASSERT_STR_EQ(thread.name, "main thread");
    ASSERT_TRUE(thread.is_main);

    ASSERT_SUCCESS(md_state_get_thread(model, 2, &thread));
    ASSERT_STR_EQ(thread.name, "worker");

    cJSON_Delete(body);
    md_state_model_destroy(model);
}

TEST(dap_stacktrace_response) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    const char *json_str = "{"
        "\"stackFrames\":["
            "{\"id\":1,\"name\":\"main\",\"line\":10,\"column\":0,"
             "\"source\":{\"path\":\"/tmp/test.c\"}},"
            "{\"id\":2,\"name\":\"func\",\"line\":20,\"column\":4,"
             "\"source\":{\"path\":\"/tmp/test.c\"}}"
        "]"
    "}";

    cJSON *body = cJSON_Parse(json_str);
    ASSERT_NOT_NULL(body);

    ASSERT_SUCCESS(md_state_parse_stacktrace_response(model, 1, body));
    ASSERT_EQ(md_state_get_stack_frame_count(model, 1), 2);

    md_stack_frame_t frame;
    ASSERT_SUCCESS(md_state_get_stack_frame(model, 1, &frame));
    ASSERT_STR_EQ(frame.name, "main");
    ASSERT_EQ(frame.line, 10);

    cJSON_Delete(body);
    md_state_model_destroy(model);
}

TEST(dap_scopes_response) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    const char *json_str = "{"
        "\"scopes\":["
            "{\"name\":\"Locals\",\"variablesReference\":1,"
             "\"namedVariables\":5,\"presentationHint\":\"locals\"},"
            "{\"name\":\"Arguments\",\"variablesReference\":2,"
             "\"namedVariables\":3,\"presentationHint\":\"arguments\"}"
        "]"
    "}";

    cJSON *body = cJSON_Parse(json_str);
    ASSERT_NOT_NULL(body);

    ASSERT_SUCCESS(md_state_parse_scopes_response(model, 1, body));

    md_scope_t scopes[2];
    int count;
    ASSERT_SUCCESS(md_state_get_scopes(model, 1, scopes, 2, &count));
    ASSERT_EQ(count, 2);
    ASSERT_STR_EQ(scopes[0].name, "Locals");
    ASSERT_EQ(scopes[0].type, MD_SCOPE_TYPE_LOCALS);
    ASSERT_EQ(scopes[0].named_variables, 5);

    cJSON_Delete(body);
    md_state_model_destroy(model);
}

TEST(dap_variables_response) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    const char *json_str = "{"
        "\"variables\":["
            "{\"name\":\"x\",\"value\":\"42\",\"type\":\"int\"},"
            "{\"name\":\"y\",\"value\":\"3.14\",\"type\":\"float\","
             "\"variablesReference\":0},"
            "{\"name\":\"arr\",\"value\":\"[3]\",\"type\":\"int[3]\","
             "\"variablesReference\":100,\"namedVariables\":0,"
             "\"indexedVariables\":3}"
        "]"
    "}";

    cJSON *body = cJSON_Parse(json_str);
    ASSERT_NOT_NULL(body);

    ASSERT_SUCCESS(md_state_parse_variables_response(model, 1, body));

    md_variable_t vars[3];
    int count;
    ASSERT_SUCCESS(md_state_get_variables(model, 1, vars, 3, &count));
    ASSERT_EQ(count, 3);
    ASSERT_STR_EQ(vars[0].name, "x");
    ASSERT_STR_EQ(vars[0].value, "42");
    ASSERT_STR_EQ(vars[0].type, "int");

    cJSON_Delete(body);
    md_state_model_destroy(model);
}

TEST(dap_breakpoints_response) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    /* Request breakpoints */
    dap_source_breakpoint_t request_bps[2];
    memset(request_bps, 0, sizeof(request_bps));
    request_bps[0].line = 10;
    request_bps[1].line = 20;

    const char *json_str = "{"
        "\"breakpoints\":["
            "{\"id\":1,\"verified\":true,\"line\":10},"
            "{\"id\":2,\"verified\":true,\"line\":20,\"message\":\"Breakpoint set\"}"
        "]"
    "}";

    cJSON *body = cJSON_Parse(json_str);
    ASSERT_NOT_NULL(body);

    ASSERT_SUCCESS(md_state_parse_set_breakpoints_response(model, "/tmp/test.c",
                                                           request_bps, 2, body));
    ASSERT_EQ(md_state_get_breakpoint_count(model), 2);

    md_breakpoint_t bp;
    ASSERT_SUCCESS(md_state_find_breakpoint_by_location(model, "/tmp/test.c", 10, &bp));
    ASSERT_TRUE(bp.verified);
    ASSERT_EQ(bp.dap_id, 1);

    cJSON_Delete(body);
    md_state_model_destroy(model);
}

/* ============================================================================
 * Test: State Listener
 * ============================================================================ */

static int g_listener_call_count = 0;
static md_state_event_t g_last_event = 0;

static void test_listener_callback(md_state_model_t *model,
                                   md_state_event_t event,
                                   void *data,
                                   void *user_data) {
    (void)model;
    (void)data;
    (void)user_data;
    g_listener_call_count++;
    g_last_event = event;
}

TEST(state_listener) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    g_listener_call_count = 0;
    g_last_event = 0;

    md_state_listener_t listener;
    listener.events = MD_STATE_EVENT_EXEC_CHANGED | MD_STATE_EVENT_BREAKPOINT_CHANGED;
    listener.callback = test_listener_callback;
    listener.user_data = NULL;

    ASSERT_SUCCESS(md_state_model_add_listener(model, &listener));

    /* Trigger execution state change */
    md_state_set_exec_state(model, MD_EXEC_STATE_RUNNING);
    ASSERT_EQ(g_listener_call_count, 1);
    ASSERT_EQ(g_last_event, MD_STATE_EVENT_EXEC_CHANGED);

    /* Trigger breakpoint change */
    md_breakpoint_t bp;
    md_breakpoint_init(&bp);
    strcpy(bp.path, "/tmp/test.c");
    bp.line = 10;
    md_state_add_breakpoint(model, &bp);
    ASSERT_EQ(g_listener_call_count, 2);
    ASSERT_EQ(g_last_event, MD_STATE_EVENT_BREAKPOINT_CHANGED);

    /* Remove listener */
    ASSERT_SUCCESS(md_state_model_remove_listener(model, test_listener_callback));

    /* Should not trigger anymore */
    md_state_set_exec_state(model, MD_EXEC_STATE_STOPPED);
    ASSERT_EQ(g_listener_call_count, 2);

    md_state_model_destroy(model);
}

/* ============================================================================
 * Test: Debug State Snapshot
 * ============================================================================ */

TEST(debug_state_snapshot) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    /* Set up some state */
    md_state_set_exec_state(model, MD_EXEC_STATE_STOPPED);
    md_state_set_stop_reason(model, MD_STOP_REASON_BREAKPOINT, "BP hit");

    md_thread_t thread;
    md_thread_init(&thread);
    thread.id = 1;
    md_state_add_thread(model, &thread);

    /* Get snapshot */
    md_debug_state_t state;
    ASSERT_SUCCESS(md_state_get_debug_state(model, &state));

    ASSERT_EQ(state.exec_state, MD_EXEC_STATE_STOPPED);
    ASSERT_EQ(state.stop_reason, MD_STOP_REASON_BREAKPOINT);
    ASSERT_STR_EQ(state.stop_description, "BP hit");
    ASSERT_EQ(state.current_thread_id, 1);

    md_state_model_destroy(model);
}

/* ============================================================================
 * Test: State Dump
 * ============================================================================ */

TEST(state_dump) {
    md_state_model_t *model = md_state_model_create(NULL);
    ASSERT_NOT_NULL(model);

    /* Add some state */
    md_state_set_exec_state(model, MD_EXEC_STATE_STOPPED);

    md_breakpoint_t bp;
    md_breakpoint_init(&bp);
    strcpy(bp.path, "/tmp/test.c");
    bp.line = 10;
    md_state_add_breakpoint(model, &bp);

    /* Dump */
    char buffer[2048];
    ASSERT_SUCCESS(md_state_dump(model, buffer, sizeof(buffer)));

    ASSERT_TRUE(strstr(buffer, "Exec State: stopped") != NULL);
    ASSERT_TRUE(strstr(buffer, "Breakpoints: 1") != NULL);
    ASSERT_TRUE(strstr(buffer, "/tmp/test.c") != NULL);

    md_state_model_destroy(model);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("\n");
    printf("========================================\n");
    printf(" Magic Debugger - Phase 3 Tests\n");
    printf(" State Model Test Suite\n");
    printf("========================================\n\n");

    /* Initialize logger */
    md_logger_config_t log_config;
    md_logger_config_init(&log_config);
    log_config.level = MD_LOG_WARNING;
    md_logger_init(&log_config);

    printf("[SUITE] State Model Lifecycle\n");
    RUN_TEST(state_model_create_destroy);
    RUN_TEST(state_model_config);
    RUN_TEST(state_model_reset);

    printf("\n[SUITE] Execution State Management\n");
    RUN_TEST(exec_state_management);
    RUN_TEST(stop_reason_management);
    RUN_TEST(thread_frame_selection);

    printf("\n[SUITE] Breakpoint Management\n");
    RUN_TEST(breakpoint_add_remove);
    RUN_TEST(breakpoint_update);
    RUN_TEST(breakpoint_enable_disable);
    RUN_TEST(breakpoint_get_all);

    printf("\n[SUITE] Thread Management\n");
    RUN_TEST(thread_add_remove);
    RUN_TEST(thread_update);

    printf("\n[SUITE] Stack Frame Management\n");
    RUN_TEST(stack_frame_management);

    printf("\n[SUITE] Variable/Scope Management\n");
    RUN_TEST(scope_management);
    RUN_TEST(variable_management);

    printf("\n[SUITE] Module Management\n");
    RUN_TEST(module_management);

    printf("\n[SUITE] Exception Management\n");
    RUN_TEST(exception_management);

    printf("\n[SUITE] DAP Event Handling\n");
    RUN_TEST(dap_stopped_event);
    RUN_TEST(dap_continued_event);
    RUN_TEST(dap_exited_event);
    RUN_TEST(dap_thread_event);

    printf("\n[SUITE] DAP Response Parsing\n");
    RUN_TEST(dap_threads_response);
    RUN_TEST(dap_stacktrace_response);
    RUN_TEST(dap_scopes_response);
    RUN_TEST(dap_variables_response);
    RUN_TEST(dap_breakpoints_response);

    printf("\n[SUITE] State Listener\n");
    RUN_TEST(state_listener);

    printf("\n[SUITE] Debug State Snapshot\n");
    RUN_TEST(debug_state_snapshot);
    RUN_TEST(state_dump);

    md_logger_shutdown();

    printf("\n========================================\n");
    printf(" All State Model tests passed!\n");
    printf("========================================\n\n");

    return 0;
}
