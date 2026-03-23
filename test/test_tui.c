/**
 * @file test_tui.c
 * @brief TUI Module Tests for Magic Debugger
 *
 * This file contains unit tests for the TUI (Terminal User Interface)
 * subsystem of Magic Debugger.
 *
 * Phase 5: Frontend - TUI Tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "tui.h"
#include "tui_panel.h"
#include "adapter.h"

/* Test counters */
static int s_tests_passed = 0;
static int s_tests_failed = 0;

/* ============================================================================
 * Test Macros
 * ============================================================================ */

#define TEST_START(name) \
    do { \
        printf("  Testing: %s... ", name); \
        fflush(stdout); \
    } while (0)

#define TEST_PASS() \
    do { \
        printf("PASSED\n"); \
        s_tests_passed++; \
    } while (0)

#define TEST_FAIL(msg) \
    do { \
        printf("FAILED: %s\n", msg); \
        s_tests_failed++; \
    } while (0)

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            TEST_FAIL(msg); \
            return; \
        } \
    } while (0)

#define ASSERT_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            TEST_FAIL(msg); \
            return; \
        } \
    } while (0)

#define ASSERT_NE(a, b, msg) \
    do { \
        if ((a) == (b)) { \
            TEST_FAIL(msg); \
            return; \
        } \
    } while (0)

#define ASSERT_STR_EQ(a, b, msg) \
    do { \
        if (strcmp((a), (b)) != 0) { \
            TEST_FAIL(msg); \
            return; \
        } \
    } while (0)

/* ============================================================================
 * TUI Configuration Tests
 * ============================================================================ */

static void test_tui_config_init(void)
{
    TEST_START("TUI config initialization");

    md_tui_config_t config;
    md_tui_config_init(&config);

    ASSERT_TRUE(config.use_colors, "use_colors should be true by default");
    ASSERT_TRUE(config.use_utf8, "use_utf8 should be true by default");
    ASSERT_TRUE(config.show_line_numbers, "show_line_numbers should be true");
    ASSERT_TRUE(config.show_breakpoint_marks, "show_breakpoint_marks should be true");
    ASSERT_EQ(config.tab_width, 4, "tab_width should be 4");
    ASSERT_EQ(config.source_context_lines, 5, "source_context_lines should be 5");
    ASSERT_TRUE(config.auto_scroll_output, "auto_scroll_output should be true");
    ASSERT_TRUE(config.follow_exec, "follow_exec should be true");
    ASSERT_TRUE(config.confirm_quit, "confirm_quit should be true");
    ASSERT_EQ(config.refresh_interval_ms, MD_TUI_REFRESH_INTERVAL,
              "refresh_interval should be default");
    ASSERT_TRUE(config.mouse_support, "mouse_support should be true");

    TEST_PASS();
}

/* ============================================================================
 * TUI Creation Tests
 * ============================================================================ */

static void test_tui_create(void)
{
    TEST_START("TUI creation");

    md_tui_t *tui = md_tui_create(NULL, NULL);
    ASSERT_NE(tui, NULL, "TUI should not be NULL");

    ASSERT_EQ(tui->initialized, false, "TUI should not be initialized");
    ASSERT_EQ(tui->running, false, "TUI should not be running");
    ASSERT_EQ(tui->adapter, NULL, "Adapter should be NULL");
    ASSERT_EQ(tui->panel_count, 0, "Panel count should be 0");
    ASSERT_EQ(tui->focused_panel, NULL, "Focused panel should be NULL");

    md_tui_destroy(tui);

    TEST_PASS();
}

static void test_tui_create_with_config(void)
{
    TEST_START("TUI creation with custom config");

    md_tui_config_t config;
    md_tui_config_init(&config);
    config.use_colors = false;
    config.tab_width = 8;
    config.confirm_quit = false;

    md_tui_t *tui = md_tui_create(NULL, &config);
    ASSERT_NE(tui, NULL, "TUI should not be NULL");

    ASSERT_EQ(tui->config.use_colors, false, "use_colors should be false");
    ASSERT_EQ(tui->config.tab_width, 8, "tab_width should be 8");
    ASSERT_EQ(tui->config.confirm_quit, false, "confirm_quit should be false");

    md_tui_destroy(tui);

    TEST_PASS();
}

/* ============================================================================
 * TUI Event Tests
 * ============================================================================ */

static void test_tui_key_mapping(void)
{
    TEST_START("TUI key mapping");

    /* Test ASCII keys - note: md_tui_get_key() requires ncurses, skip in stub mode */
    /* ASSERT_EQ(md_tui_get_key(), 0, "get_key returns 0 in stub mode"); */

    /* Test key name function */
    ASSERT_STR_EQ(md_tui_key_name(MD_KEY_UP), "Up", "Up key name");
    ASSERT_STR_EQ(md_tui_key_name(MD_KEY_DOWN), "Down", "Down key name");
    ASSERT_STR_EQ(md_tui_key_name(MD_KEY_LEFT), "Left", "Left key name");
    ASSERT_STR_EQ(md_tui_key_name(MD_KEY_RIGHT), "Right", "Right key name");
    ASSERT_STR_EQ(md_tui_key_name(MD_KEY_F1), "F1", "F1 key name");
    ASSERT_STR_EQ(md_tui_key_name(MD_KEY_F5), "F5", "F5 key name");
    ASSERT_STR_EQ(md_tui_key_name(MD_KEY_ENTER), "Enter", "Enter key name");
    ASSERT_STR_EQ(md_tui_key_name(MD_KEY_ESCAPE), "Escape", "Escape key name");

    /* Test control key detection */
    ASSERT_EQ(md_tui_is_ctrl_key(1), true, "Ctrl-A is control key");
    ASSERT_EQ(md_tui_is_ctrl_key(65), false, "'A' is not control key");

    TEST_PASS();
}

static void test_tui_command_name(void)
{
    TEST_START("TUI command name");

    ASSERT_STR_EQ(md_tui_command_name(MD_TUI_CMD_CONTINUE), "continue",
                  "Continue command name");
    ASSERT_STR_EQ(md_tui_command_name(MD_TUI_CMD_STEP_OVER), "next",
                  "Step over command name");
    ASSERT_STR_EQ(md_tui_command_name(MD_TUI_CMD_STEP_INTO), "step",
                  "Step into command name");
    ASSERT_STR_EQ(md_tui_command_name(MD_TUI_CMD_TOGGLE_BREAKPOINT), "break",
                  "Toggle breakpoint command name");
    ASSERT_STR_EQ(md_tui_command_name(MD_TUI_CMD_QUIT), "quit",
                  "Quit command name");

    TEST_PASS();
}

/* ============================================================================
 * Panel Creation Tests
 * ============================================================================ */

static void test_source_panel_create(void)
{
    TEST_START("Source panel creation");

    md_tui_panel_t *panel = md_source_panel_create();
    ASSERT_NE(panel, NULL, "Panel should not be NULL");

    ASSERT_EQ(panel->type, MD_TUI_PANEL_SOURCE, "Panel type should be SOURCE");
    ASSERT_STR_EQ(panel->title, "Source", "Panel title should be 'Source'");
    ASSERT_TRUE(panel->resizable, "Panel should be resizable");
    ASSERT_EQ(panel->visibility, MD_TUI_VISIBLE, "Panel should be visible");

    /* Cleanup */
    if (panel->vtable && panel->vtable->destroy) {
        panel->vtable->destroy(panel);
    }
    free(panel);

    TEST_PASS();
}

static void test_breakpoint_panel_create(void)
{
    TEST_START("Breakpoint panel creation");

    md_tui_panel_t *panel = md_breakpoint_panel_create();
    ASSERT_NE(panel, NULL, "Panel should not be NULL");

    ASSERT_EQ(panel->type, MD_TUI_PANEL_BREAKPOINTS, "Panel type should be BREAKPOINTS");
    ASSERT_STR_EQ(panel->title, "Breakpoints", "Panel title should be 'Breakpoints'");
    ASSERT_TRUE(panel->resizable, "Panel should be resizable");

    if (panel->vtable && panel->vtable->destroy) {
        panel->vtable->destroy(panel);
    }
    free(panel);

    TEST_PASS();
}

static void test_callstack_panel_create(void)
{
    TEST_START("Call stack panel creation");

    md_tui_panel_t *panel = md_callstack_panel_create();
    ASSERT_NE(panel, NULL, "Panel should not be NULL");

    ASSERT_EQ(panel->type, MD_TUI_PANEL_CALLSTACK, "Panel type should be CALLSTACK");
    ASSERT_STR_EQ(panel->title, "Call Stack", "Panel title should be 'Call Stack'");

    if (panel->vtable && panel->vtable->destroy) {
        panel->vtable->destroy(panel);
    }
    free(panel);

    TEST_PASS();
}

static void test_variable_panel_create(void)
{
    TEST_START("Variable panel creation");

    md_tui_panel_t *panel = md_variable_panel_create();
    ASSERT_NE(panel, NULL, "Panel should not be NULL");

    ASSERT_EQ(panel->type, MD_TUI_PANEL_VARIABLES, "Panel type should be VARIABLES");
    ASSERT_STR_EQ(panel->title, "Variables", "Panel title should be 'Variables'");

    if (panel->vtable && panel->vtable->destroy) {
        panel->vtable->destroy(panel);
    }
    free(panel);

    TEST_PASS();
}

static void test_output_panel_create(void)
{
    TEST_START("Output panel creation");

    md_tui_panel_t *panel = md_output_panel_create();
    ASSERT_NE(panel, NULL, "Panel should not be NULL");

    ASSERT_EQ(panel->type, MD_TUI_PANEL_OUTPUT, "Panel type should be OUTPUT");
    ASSERT_STR_EQ(panel->title, "Output", "Panel title should be 'Output'");

    if (panel->vtable && panel->vtable->destroy) {
        panel->vtable->destroy(panel);
    }
    free(panel);

    TEST_PASS();
}

static void test_thread_panel_create(void)
{
    TEST_START("Thread panel creation");

    md_tui_panel_t *panel = md_thread_panel_create();
    ASSERT_NE(panel, NULL, "Panel should not be NULL");

    ASSERT_EQ(panel->type, MD_TUI_PANEL_THREADS, "Panel type should be THREADS");
    ASSERT_STR_EQ(panel->title, "Threads", "Panel title should be 'Threads'");

    if (panel->vtable && panel->vtable->destroy) {
        panel->vtable->destroy(panel);
    }
    free(panel);

    TEST_PASS();
}

static void test_status_panel_create(void)
{
    TEST_START("Status panel creation");

    md_tui_panel_t *panel = md_status_panel_create();
    ASSERT_NE(panel, NULL, "Panel should not be NULL");

    ASSERT_EQ(panel->type, MD_TUI_PANEL_STATUS, "Panel type should be STATUS");
    ASSERT_EQ(panel->resizable, false, "Status panel should not be resizable");

    if (panel->vtable && panel->vtable->destroy) {
        panel->vtable->destroy(panel);
    }
    free(panel);

    TEST_PASS();
}

static void test_help_panel_create(void)
{
    TEST_START("Help panel creation");

    md_tui_panel_t *panel = md_help_panel_create();
    ASSERT_NE(panel, NULL, "Panel should not be NULL");

    ASSERT_EQ(panel->type, MD_TUI_PANEL_HELP, "Panel type should be HELP");
    ASSERT_STR_EQ(panel->title, "Help", "Panel title should be 'Help'");
    ASSERT_EQ(panel->visibility, MD_TUI_HIDDEN, "Help panel should be hidden by default");

    if (panel->vtable && panel->vtable->destroy) {
        panel->vtable->destroy(panel);
    }
    free(panel);

    TEST_PASS();
}

static void test_panel_factory(void)
{
    TEST_START("Panel factory");

    md_tui_panel_t *panel;

    panel = md_tui_panel_create_by_type(MD_TUI_PANEL_SOURCE);
    ASSERT_NE(panel, NULL, "Should create source panel");
    if (panel) {
        if (panel->vtable && panel->vtable->destroy) panel->vtable->destroy(panel);
        free(panel);
    }

    panel = md_tui_panel_create_by_type(MD_TUI_PANEL_BREAKPOINTS);
    ASSERT_NE(panel, NULL, "Should create breakpoint panel");
    if (panel) {
        if (panel->vtable && panel->vtable->destroy) panel->vtable->destroy(panel);
        free(panel);
    }

    panel = md_tui_panel_create_by_type(MD_TUI_PANEL_CALLSTACK);
    ASSERT_NE(panel, NULL, "Should create callstack panel");
    if (panel) {
        if (panel->vtable && panel->vtable->destroy) panel->vtable->destroy(panel);
        free(panel);
    }

    panel = md_tui_panel_create_by_type(MD_TUI_PANEL_VARIABLES);
    ASSERT_NE(panel, NULL, "Should create variable panel");
    if (panel) {
        if (panel->vtable && panel->vtable->destroy) panel->vtable->destroy(panel);
        free(panel);
    }

    panel = md_tui_panel_create_by_type(MD_TUI_PANEL_OUTPUT);
    ASSERT_NE(panel, NULL, "Should create output panel");
    if (panel) {
        if (panel->vtable && panel->vtable->destroy) panel->vtable->destroy(panel);
        free(panel);
    }

    panel = md_tui_panel_create_by_type(MD_TUI_PANEL_STATUS);
    ASSERT_NE(panel, NULL, "Should create status panel");
    if (panel) {
        if (panel->vtable && panel->vtable->destroy) panel->vtable->destroy(panel);
        free(panel);
    }

    panel = md_tui_panel_create_by_type(MD_TUI_PANEL_HELP);
    ASSERT_NE(panel, NULL, "Should create help panel");
    if (panel) {
        if (panel->vtable && panel->vtable->destroy) panel->vtable->destroy(panel);
        free(panel);
    }

    TEST_PASS();
}

/* ============================================================================
 * Source Panel Tests
 * ============================================================================ */

static void test_source_panel_file_loading(void)
{
    TEST_START("Source panel file loading");

    md_tui_panel_t *panel = md_source_panel_create();
    ASSERT_NE(panel, NULL, "Panel should not be NULL");

    /* Initialize panel */
    if (panel->vtable && panel->vtable->init) {
        md_error_t err = panel->vtable->init(panel);
        ASSERT_EQ(err, MD_SUCCESS, "Panel initialization should succeed");
    }

    /* Test loading a non-existent file */
    md_error_t err = md_source_panel_load_file(panel, "/nonexistent/file.c");
    ASSERT_EQ(err, MD_ERROR_NOT_FOUND, "Should return NOT_FOUND for missing file");

    /* Test loading an existing file */
    err = md_source_panel_load_file(panel, __FILE__);
    ASSERT_EQ(err, MD_SUCCESS, "Should load current test file");

    md_source_panel_data_t *data = (md_source_panel_data_t*)panel->user_data;
    ASSERT_NE(data, NULL, "Panel data should not be NULL");
    ASSERT_TRUE(data->line_count > 0, "Should have loaded lines");
    ASSERT_STR_EQ(data->file_path, __FILE__, "File path should match");

    /* Test scroll functions */
    md_source_panel_set_current_line(panel, 10);
    ASSERT_EQ(data->current_line, 10, "Current line should be set");

    md_source_panel_scroll_to_line(panel, 50, true);
    /* Verify scroll position (would need actual ncurses for visual test) */

    /* Cleanup */
    if (panel->vtable && panel->vtable->destroy) {
        panel->vtable->destroy(panel);
    }
    free(panel);

    TEST_PASS();
}

/* ============================================================================
 * Output Panel Tests
 * ============================================================================ */

static void test_output_panel_output(void)
{
    TEST_START("Output panel output");

    md_tui_panel_t *panel = md_output_panel_create();
    ASSERT_NE(panel, NULL, "Panel should not be NULL");

    /* Initialize panel */
    if (panel->vtable && panel->vtable->init) {
        md_error_t err = panel->vtable->init(panel);
        ASSERT_EQ(err, MD_SUCCESS, "Panel initialization should succeed");
    }

    /* Create a mock TUI context for the panel */
    md_tui_t mock_tui;
    memset(&mock_tui, 0, sizeof(mock_tui));
    panel->tui = &mock_tui;
    panel->rect.height = 20;
    panel->rect.width = 80;

    /* Test adding output lines */
    md_output_panel_add_line(panel, MD_OUTPUT_STDOUT, "Test output line 1");
    md_output_panel_add_line(panel, MD_OUTPUT_STDOUT, "Test output line 2");
    md_output_panel_add_line(panel, MD_OUTPUT_ERROR, "Error message");
    md_output_panel_add_line(panel, MD_OUTPUT_COMMAND, "> continue");

    md_output_panel_data_t *data = (md_output_panel_data_t*)panel->user_data;
    ASSERT_NE(data, NULL, "Panel data should not be NULL");
    ASSERT_EQ(data->count, 4, "Should have 4 output lines");

    /* Test clear */
    md_output_panel_clear(panel);
    ASSERT_EQ(data->count, 0, "Output should be cleared");

    /* Cleanup */
    if (panel->vtable && panel->vtable->destroy) {
        panel->vtable->destroy(panel);
    }
    free(panel);

    TEST_PASS();
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

static void test_panel_type_name(void)
{
    TEST_START("Panel type name");

    ASSERT_STR_EQ(md_tui_panel_type_name(MD_TUI_PANEL_SOURCE), "Source",
                  "Source panel name");
    ASSERT_STR_EQ(md_tui_panel_type_name(MD_TUI_PANEL_BREAKPOINTS), "Breakpoints",
                  "Breakpoints panel name");
    ASSERT_STR_EQ(md_tui_panel_type_name(MD_TUI_PANEL_CALLSTACK), "Call Stack",
                  "Call stack panel name");
    ASSERT_STR_EQ(md_tui_panel_type_name(MD_TUI_PANEL_VARIABLES), "Variables",
                  "Variables panel name");
    ASSERT_STR_EQ(md_tui_panel_type_name(MD_TUI_PANEL_OUTPUT), "Output",
                  "Output panel name");
    ASSERT_STR_EQ(md_tui_panel_type_name(MD_TUI_PANEL_THREADS), "Threads",
                  "Threads panel name");

    TEST_PASS();
}

static void test_breakpoint_marker_format(void)
{
    TEST_START("Breakpoint marker format");

    char buffer[16];

    /* Current line marker */
    md_tui_format_breakpoint_marker(0, true, false, buffer, sizeof(buffer));
    ASSERT_STR_EQ(buffer, ">", "Current line marker");

    /* Enabled breakpoint */
    md_tui_format_breakpoint_marker(1, false, true, buffer, sizeof(buffer));
    ASSERT_STR_EQ(buffer, "B", "Enabled breakpoint marker");

    /* Disabled breakpoint */
    md_tui_format_breakpoint_marker(2, false, false, buffer, sizeof(buffer));
    ASSERT_STR_EQ(buffer, "b", "Disabled breakpoint marker");

    /* No breakpoint */
    md_tui_format_breakpoint_marker(0, false, true, buffer, sizeof(buffer));
    ASSERT_STR_EQ(buffer, " ", "No marker for empty line");

    TEST_PASS();
}

static void test_color_helpers(void)
{
    TEST_START("Color helper functions");

    /* Breakpoint colors */
    ASSERT_EQ(md_tui_get_breakpoint_color(NULL, true), MD_TUI_COLOR_BREAKPOINT,
              "Enabled breakpoint color");
    ASSERT_EQ(md_tui_get_breakpoint_color(NULL, false), MD_TUI_COLOR_DISABLED,
              "Disabled breakpoint color");

    /* Thread colors */
    ASSERT_EQ(md_tui_get_thread_color(NULL, MD_THREAD_STATE_RUNNING),
              MD_TUI_COLOR_SUCCESS, "Running thread color");
    ASSERT_EQ(md_tui_get_thread_color(NULL, MD_THREAD_STATE_STOPPED),
              MD_TUI_COLOR_WARNING, "Stopped thread color");
    ASSERT_EQ(md_tui_get_thread_color(NULL, MD_THREAD_STATE_EXITED),
              MD_TUI_COLOR_DISABLED, "Exited thread color");

    TEST_PASS();
}

/* ============================================================================
 * TUI Panel Management Tests (without ncurses)
 * ============================================================================ */

static void test_tui_panel_management(void)
{
    TEST_START("TUI panel management");

    md_tui_t *tui = md_tui_create(NULL, NULL);
    ASSERT_NE(tui, NULL, "TUI should not be NULL");

    /* Create panels manually - note: we don't add them to TUI's panel list
     * to avoid double-free on cleanup. Just test panel creation. */
    md_tui_panel_t *source = md_source_panel_create();
    md_tui_panel_t *output = md_output_panel_create();
    md_tui_panel_t *status = md_status_panel_create();

    ASSERT_NE(source, NULL, "Source panel should not be NULL");
    ASSERT_NE(output, NULL, "Output panel should not be NULL");
    ASSERT_NE(status, NULL, "Status panel should not be NULL");

    /* Test panel types */
    ASSERT_EQ(source->type, MD_TUI_PANEL_SOURCE, "Source panel type");
    ASSERT_EQ(output->type, MD_TUI_PANEL_OUTPUT, "Output panel type");
    ASSERT_EQ(status->type, MD_TUI_PANEL_STATUS, "Status panel type");

    /* Cleanup panels manually */
    if (source->vtable && source->vtable->destroy) source->vtable->destroy(source);
    if (output->vtable && output->vtable->destroy) output->vtable->destroy(output);
    if (status->vtable && status->vtable->destroy) status->vtable->destroy(status);
    free(source);
    free(output);
    free(status);
    
    /* Cleanup TUI */
    md_tui_destroy(tui);

    TEST_PASS();
}

/* ============================================================================
 * Command History Tests
 * ============================================================================ */

static void test_command_history(void)
{
    TEST_START("Command history");

    md_tui_t *tui = md_tui_create(NULL, NULL);
    ASSERT_NE(tui, NULL, "TUI should not be NULL");

    /* Add commands to history */
    md_tui_add_history(tui, "continue");
    md_tui_add_history(tui, "next");
    md_tui_add_history(tui, "step");

    ASSERT_EQ(tui->cmd_history_count, 3, "Should have 3 history entries");

    /* Test retrieval */
    ASSERT_STR_EQ(md_tui_get_history(tui, 0), "continue", "First history entry");
    ASSERT_STR_EQ(md_tui_get_history(tui, 1), "next", "Second history entry");
    ASSERT_STR_EQ(md_tui_get_history(tui, -1), "step", "Last history entry (negative index)");

    /* Test duplicate prevention */
    md_tui_add_history(tui, "step");
    ASSERT_EQ(tui->cmd_history_count, 3, "Should not add duplicate");

    /* Test clear */
    md_tui_clear_history(tui);
    ASSERT_EQ(tui->cmd_history_count, 0, "History should be cleared");

    md_tui_destroy(tui);

    TEST_PASS();
}

/* ============================================================================
 * Message Display Tests
 * ============================================================================ */

static void test_message_display(void)
{
    TEST_START("Message display");

    md_tui_t *tui = md_tui_create(NULL, NULL);
    ASSERT_NE(tui, NULL, "TUI should not be NULL");

    /* Set up minimal context */
    tui->screen_width = 80;
    tui->screen_height = 24;

    /* Show message */
    md_tui_show_message(tui, MD_TUI_COLOR_SUCCESS, "Test message %d", 42);
    ASSERT_TRUE(strlen(tui->message) > 0, "Message should be set");
    ASSERT_EQ(tui->message_color, MD_TUI_COLOR_SUCCESS, "Message color should be set");
    ASSERT_TRUE(strstr(tui->message, "42") != NULL, "Message should contain formatted value");

    /* Clear message */
    md_tui_clear_message(tui);
    ASSERT_EQ(tui->message[0], '\0', "Message should be cleared");

    md_tui_destroy(tui);

    TEST_PASS();
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(void)
{
    printf("\n");
    printf("==============================================\n");
    printf("  Magic Debugger - Phase 5: Frontend Tests\n");
    printf("==============================================\n\n");

    /* Configuration tests */
    printf("Configuration Tests:\n");
    test_tui_config_init();
    printf("\n");

    /* Creation tests */
    printf("Creation Tests:\n");
    test_tui_create();
    test_tui_create_with_config();
    printf("\n");

    /* Event tests */
    printf("Event Tests:\n");
    test_tui_key_mapping();
    test_tui_command_name();
    printf("\n");

    /* Panel creation tests */
    printf("Panel Creation Tests:\n");
    test_source_panel_create();
    test_breakpoint_panel_create();
    test_callstack_panel_create();
    test_variable_panel_create();
    test_output_panel_create();
    test_thread_panel_create();
    test_status_panel_create();
    test_help_panel_create();
    test_panel_factory();
    printf("\n");

    /* Panel functionality tests */
    printf("Panel Functionality Tests:\n");
    test_source_panel_file_loading();
    test_output_panel_output();
    printf("\n");

    /* Utility tests */
    printf("Utility Tests:\n");
    test_panel_type_name();
    test_breakpoint_marker_format();
    test_color_helpers();
    printf("\n");

    /* TUI management tests */
    printf("TUI Management Tests:\n");
    test_tui_panel_management();
    test_command_history();
    test_message_display();
    printf("\n");

    /* Summary */
    printf("==============================================\n");
    printf("  Test Summary\n");
    printf("==============================================\n");
    printf("  Passed: %d\n", s_tests_passed);
    printf("  Failed: %d\n", s_tests_failed);
    printf("  Total:  %d\n", s_tests_passed + s_tests_failed);
    printf("==============================================\n\n");

    if (s_tests_failed > 0) {
        printf("SOME TESTS FAILED!\n\n");
        return 1;
    }

    printf("ALL TESTS PASSED!\n\n");
    return 0;
}
