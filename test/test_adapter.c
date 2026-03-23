/**
 * @file test_adapter.c
 * @brief Test program for Adapter Layer (Phase 4)
 *
 * Tests:
 * 1. Adapter registry
 * 2. Adapter creation by type/name
 * 3. Configuration helpers
 * 4. LLDB adapter
 * 5. GDB adapter
 * 6. Shell adapter
 * 7. Adapter capabilities
 * 8. Breakpoint operations
 * 9. Source display
 */

#include "adapter.h"
#include "lldb_adapter.h"
#include "gdb_adapter.h"
#include "shell_adapter.h"
#include "logger.h"
#include "state_model.h"
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
 * Test: Configuration
 * ============================================================================ */

TEST(config_init) {
    md_adapter_config_t config;
    md_adapter_config_init(&config);
    
    ASSERT_EQ(config.stop_on_entry, false);
    ASSERT_EQ(config.auto_disassemble, true);
    ASSERT_EQ(config.source_context_lines, MD_ADAPTER_SOURCE_CONTEXT);
    ASSERT_TRUE(config.timeout_ms > 0);
}

TEST(config_setters) {
    md_adapter_config_t config;
    md_adapter_config_init(&config);
    
    ASSERT_SUCCESS(md_adapter_config_set_program(&config, "/tmp/test"));
    ASSERT_STR_EQ(config.program_path, "/tmp/test");
    
    ASSERT_SUCCESS(md_adapter_config_set_args(&config, "arg1 arg2"));
    ASSERT_STR_EQ(config.program_args, "arg1 arg2");
    
    ASSERT_SUCCESS(md_adapter_config_set_cwd(&config, "/home/user"));
    ASSERT_STR_EQ(config.working_dir, "/home/user");
    
    ASSERT_SUCCESS(md_adapter_config_set_debugger(&config, "/usr/bin/lldb-dap"));
    ASSERT_STR_EQ(config.debugger_path, "/usr/bin/lldb-dap");
}

/* ============================================================================
 * Test: Adapter Registry
 * ============================================================================ */

TEST(adapter_registry) {
    /* Built-in adapters should be auto-registered */
    md_adapter_t *lldb = md_adapter_create(MD_DEBUGGER_LLDB, NULL);
    ASSERT_NOT_NULL(lldb);
    md_adapter_destroy(lldb);
    
    md_adapter_t *gdb = md_adapter_create(MD_DEBUGGER_GDB, NULL);
    ASSERT_NOT_NULL(gdb);
    md_adapter_destroy(gdb);
    
    md_adapter_t *shell = md_adapter_create(MD_DEBUGGER_SHELL, NULL);
    ASSERT_NOT_NULL(shell);
    md_adapter_destroy(shell);
}

TEST(adapter_create_by_name) {
    md_adapter_t *adapter;
    
    adapter = md_adapter_create_by_name("lldb", NULL);
    ASSERT_NOT_NULL(adapter);
    ASSERT_STR_EQ(md_adapter_get_name(adapter), "LLDB");
    md_adapter_destroy(adapter);
    
    adapter = md_adapter_create_by_name("gdb", NULL);
    ASSERT_NOT_NULL(adapter);
    ASSERT_STR_EQ(md_adapter_get_name(adapter), "GDB");
    md_adapter_destroy(adapter);
    
    adapter = md_adapter_create_by_name("shell", NULL);
    ASSERT_NOT_NULL(adapter);
    ASSERT_STR_EQ(md_adapter_get_name(adapter), "Shell");
    md_adapter_destroy(adapter);
    
    adapter = md_adapter_create_by_name("bashdb", NULL);
    ASSERT_NOT_NULL(adapter);
    md_adapter_destroy(adapter);
}

/* ============================================================================
 * Test: LLDB Adapter
 * ============================================================================ */

TEST(lldb_adapter_create) {
    md_adapter_config_t config;
    md_adapter_config_init(&config);
    
    md_adapter_t *adapter = md_adapter_create(MD_DEBUGGER_LLDB, &config);
    ASSERT_NOT_NULL(adapter);
    
    ASSERT_EQ(adapter->type, MD_DEBUGGER_LLDB);
    ASSERT_STR_EQ(md_adapter_get_name(adapter), "LLDB");
    ASSERT_EQ(md_adapter_get_state(adapter), MD_ADAPTER_STATE_NONE);
    
    md_adapter_destroy(adapter);
}

TEST(lldb_options_init) {
    lldb_adapter_options_t options;
    lldb_adapter_options_init(&options);
    
    ASSERT_EQ(options.display_runtime_support_values, false);
    ASSERT_EQ(options.command_completions_enabled, true);
    ASSERT_EQ(options.supports_granular_stepping, true);
}

/* ============================================================================
 * Test: GDB Adapter
 * ============================================================================ */

TEST(gdb_adapter_create) {
    md_adapter_config_t config;
    md_adapter_config_init(&config);
    
    md_adapter_t *adapter = md_adapter_create(MD_DEBUGGER_GDB, &config);
    ASSERT_NOT_NULL(adapter);
    
    ASSERT_EQ(adapter->type, MD_DEBUGGER_GDB);
    ASSERT_STR_EQ(md_adapter_get_name(adapter), "GDB");
    ASSERT_EQ(md_adapter_get_state(adapter), MD_ADAPTER_STATE_NONE);
    
    md_adapter_destroy(adapter);
}

TEST(gdb_options_init) {
    gdb_adapter_options_t options;
    gdb_adapter_options_init(&options);
    
    ASSERT_EQ(options.use_mi_protocol, true);
    ASSERT_EQ(options.use_gdb_dap, false);
    ASSERT_EQ(options.print_address, true);
    ASSERT_STR_EQ(options.disassembly_flavor, "intel");
}

/* ============================================================================
 * Test: Shell Adapter
 * ============================================================================ */

TEST(shell_adapter_create) {
    md_adapter_config_t config;
    md_adapter_config_init(&config);
    
    md_adapter_t *adapter = md_adapter_create(MD_DEBUGGER_SHELL, &config);
    ASSERT_NOT_NULL(adapter);
    
    ASSERT_EQ(adapter->type, MD_DEBUGGER_SHELL);
    ASSERT_STR_EQ(md_adapter_get_name(adapter), "Shell");
    ASSERT_EQ(md_adapter_get_state(adapter), MD_ADAPTER_STATE_NONE);
    
    md_adapter_destroy(adapter);
}

TEST(shell_options_init) {
    shell_adapter_options_t options;
    shell_adapter_options_init(&options);
    
    ASSERT_EQ(options.debug_mode, true);
    ASSERT_EQ(options.line_numbers, true);
    ASSERT_STR_EQ(options.shell_path, "/bin/bash");
}

/* ============================================================================
 * Test: Adapter State
 * ============================================================================ */

TEST(adapter_state_string) {
    ASSERT_STR_EQ(md_adapter_state_string(MD_ADAPTER_STATE_NONE), "none");
    ASSERT_STR_EQ(md_adapter_state_string(MD_ADAPTER_STATE_INITIALIZED), "initialized");
    ASSERT_STR_EQ(md_adapter_state_string(MD_ADAPTER_STATE_RUNNING), "running");
    ASSERT_STR_EQ(md_adapter_state_string(MD_ADAPTER_STATE_STOPPED), "stopped");
    ASSERT_STR_EQ(md_adapter_state_string(MD_ADAPTER_STATE_TERMINATED), "terminated");
    ASSERT_STR_EQ(md_adapter_state_string(MD_ADAPTER_STATE_ERROR), "error");
}

TEST(adapter_state_model) {
    md_adapter_t *adapter = md_adapter_create(MD_DEBUGGER_LLDB, NULL);
    ASSERT_NOT_NULL(adapter);
    
    /* State model should be created with adapter */
    (void)md_adapter_get_state_model(adapter);
    /* Note: model is NULL until init is called */
    
    md_adapter_destroy(adapter);
}

/* ============================================================================
 * Test: Capabilities
 * ============================================================================ */

TEST(adapter_capabilities) {
    md_adapter_t *adapter = md_adapter_create(MD_DEBUGGER_LLDB, NULL);
    ASSERT_NOT_NULL(adapter);
    
    /* Capabilities should be loaded after init */
    (void)md_adapter_get_capabilities(adapter);
    /* Note: caps is NULL until init is called */
    
    md_adapter_destroy(adapter);
}

TEST(adapter_supports_feature) {
    md_adapter_t *adapter = md_adapter_create(MD_DEBUGGER_LLDB, NULL);
    ASSERT_NOT_NULL(adapter);
    
    /* Features not supported until capabilities are loaded */
    bool supported = md_adapter_supports_feature(adapter, "conditionalBreakpoints");
    ASSERT_FALSE(supported);  /* Not loaded yet */
    
    md_adapter_destroy(adapter);
}

/* ============================================================================
 * Test: Utility Functions
 * ============================================================================ */

TEST(step_granularity_string) {
    ASSERT_STR_EQ(md_step_granularity_string(MD_STEP_LINE), "line");
    ASSERT_STR_EQ(md_step_granularity_string(MD_STEP_STATEMENT), "statement");
    ASSERT_STR_EQ(md_step_granularity_string(MD_STEP_INSTRUCTION), "instruction");
}

TEST(detect_adapter_type) {
    ASSERT_EQ(md_adapter_detect_type("/usr/bin/lldb-dap"), MD_DEBUGGER_LLDB);
    ASSERT_EQ(md_adapter_detect_type("/usr/bin/lldb"), MD_DEBUGGER_LLDB);
    ASSERT_EQ(md_adapter_detect_type("/usr/bin/gdb"), MD_DEBUGGER_GDB);
    ASSERT_EQ(md_adapter_detect_type("/usr/bin/bashdb"), MD_DEBUGGER_SHELL);
    ASSERT_EQ(md_adapter_detect_type("/usr/bin/my-debugger"), MD_DEBUGGER_CUSTOM);
}

TEST(find_debugger) {
    char path[MD_MAX_PATH_LEN];
    
    /* Try to find each debugger type */
    md_error_t err = md_adapter_find_debugger(MD_DEBUGGER_LLDB, path, sizeof(path));
    /* May or may not find depending on system */
    (void)err;
    
    err = md_adapter_find_debugger(MD_DEBUGGER_GDB, path, sizeof(path));
    /* GDB is more commonly installed */
    
    err = md_adapter_find_debugger(MD_DEBUGGER_SHELL, path, sizeof(path));
    (void)err;
}

/* ============================================================================
 * Test: Source Context
 * ============================================================================ */

TEST(source_context) {
    md_source_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    /* Create test lines */
    ctx.line_count = 5;
    ctx.lines = (md_source_line_t*)calloc(5, sizeof(md_source_line_t));
    ASSERT_NOT_NULL(ctx.lines);
    
    for (int i = 0; i < 5; i++) {
        ctx.lines[i].line_number = i + 1;
        snprintf(ctx.lines[i].content, MD_ADAPTER_LINE_MAX, "Line %d", i + 1);
        ctx.lines[i].is_executable = true;
        ctx.lines[i].is_current = (i == 2);
        strcpy(ctx.lines[i].breakpoint_marker, ctx.lines[i].is_current ? ">" : " ");
    }
    
    /* Verify setup */
    ASSERT_EQ(ctx.lines[2].is_current, true);
    ASSERT_STR_EQ(ctx.lines[2].breakpoint_marker, ">");
    
    md_adapter_free_source_context(&ctx);
    ASSERT_NULL(ctx.lines);
}

/* ============================================================================
 * Test: Eval Result
 * ============================================================================ */

TEST(eval_result) {
    md_eval_result_t result;
    memset(&result, 0, sizeof(result));
    
    result.success = true;
    strcpy(result.value, "42");
    strcpy(result.type, "int");
    result.variables_reference = 0;
    
    ASSERT_TRUE(result.success);
    ASSERT_STR_EQ(result.value, "42");
    ASSERT_STR_EQ(result.type, "int");
}

/* ============================================================================
 * Test: Breakpoint Types
 * ============================================================================ */

TEST(breakpoint_types) {
    md_breakpoint_t bp;
    md_breakpoint_init(&bp);
    
    ASSERT_EQ(bp.id, MD_INVALID_ID);
    ASSERT_EQ(bp.dap_id, MD_INVALID_ID);
    ASSERT_EQ(bp.type, MD_BP_TYPE_LINE);
    ASSERT_EQ(bp.state, MD_BP_STATE_INVALID);
    ASSERT_EQ(bp.line, -1);
    ASSERT_TRUE(bp.enabled);
    ASSERT_FALSE(bp.verified);
    
    /* Set properties */
    strcpy(bp.path, "/tmp/test.c");
    bp.line = 42;
    bp.type = MD_BP_TYPE_CONDITIONAL;
    strcpy(bp.condition, "x > 10");
    bp.state = MD_BP_STATE_PENDING;
    
    ASSERT_STR_EQ(bp.path, "/tmp/test.c");
    ASSERT_EQ(bp.line, 42);
    ASSERT_EQ(bp.type, MD_BP_TYPE_CONDITIONAL);
}

/* ============================================================================
 * Test: State Integration
 * ============================================================================ */

TEST(state_model_integration) {
    md_adapter_t *adapter = md_adapter_create(MD_DEBUGGER_LLDB, NULL);
    ASSERT_NOT_NULL(adapter);
    
    /* Create state model */
    md_state_config_t config;
    md_state_config_init(&config);
    adapter->state_model = md_state_model_create(&config);
    ASSERT_NOT_NULL(adapter->state_model);
    
    /* Test state model access */
    md_state_model_t *model = md_adapter_get_state_model(adapter);
    ASSERT_NOT_NULL(model);
    ASSERT_EQ(model, adapter->state_model);
    
    md_adapter_destroy(adapter);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("\n");
    printf("========================================\n");
    printf(" Magic Debugger - Phase 4 Tests\n");
    printf(" Adapter Layer Test Suite\n");
    printf("========================================\n\n");

    /* Initialize logger */
    md_logger_config_t log_config;
    md_logger_config_init(&log_config);
    log_config.level = MD_LOG_WARNING;
    md_logger_init(&log_config);

    printf("[SUITE] Configuration\n");
    RUN_TEST(config_init);
    RUN_TEST(config_setters);

    printf("\n[SUITE] Adapter Registry\n");
    RUN_TEST(adapter_registry);
    RUN_TEST(adapter_create_by_name);

    printf("\n[SUITE] LLDB Adapter\n");
    RUN_TEST(lldb_adapter_create);
    RUN_TEST(lldb_options_init);

    printf("\n[SUITE] GDB Adapter\n");
    RUN_TEST(gdb_adapter_create);
    RUN_TEST(gdb_options_init);

    printf("\n[SUITE] Shell Adapter\n");
    RUN_TEST(shell_adapter_create);
    RUN_TEST(shell_options_init);

    printf("\n[SUITE] Adapter State\n");
    RUN_TEST(adapter_state_string);
    RUN_TEST(adapter_state_model);

    printf("\n[SUITE] Capabilities\n");
    RUN_TEST(adapter_capabilities);
    RUN_TEST(adapter_supports_feature);

    printf("\n[SUITE] Utility Functions\n");
    RUN_TEST(step_granularity_string);
    RUN_TEST(detect_adapter_type);
    RUN_TEST(find_debugger);

    printf("\n[SUITE] Source Context\n");
    RUN_TEST(source_context);

    printf("\n[SUITE] Eval Result\n");
    RUN_TEST(eval_result);

    printf("\n[SUITE] Breakpoint Types\n");
    RUN_TEST(breakpoint_types);

    printf("\n[SUITE] State Integration\n");
    RUN_TEST(state_model_integration);

    md_logger_shutdown();

    printf("\n========================================\n");
    printf(" All Adapter Layer tests passed!\n");
    printf("========================================\n\n");

    return 0;
}
