/**
 * @file test_session.c
 * @brief Test program for Session Manager (Phase 1)
 * 
 * Tests:
 * 1. Session manager creation/destruction
 * 2. Session creation with cat (simple echo test)
 * 3. Process communication (send/receive)
 * 4. Process termination
 * 5. Signal handling
 */

#include "types.h"
#include "logger.h"
#include "io_handler.h"
#include "signal_handler.h"
#include "session_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
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

/* ============================================================================
 * Test: Error Strings
 * ============================================================================ */

TEST(error_strings) {
    const char *str;
    
    str = md_error_string(MD_SUCCESS);
    ASSERT_NOT_NULL(str);
    ASSERT_TRUE(strlen(str) > 0);
    
    str = md_error_string(MD_ERROR_UNKNOWN);
    ASSERT_NOT_NULL(str);
    
    str = md_error_string(MD_ERROR_INVALID_ARG);
    ASSERT_NOT_NULL(str);
    
    str = md_error_string((md_error_t)-9999);
    ASSERT_NOT_NULL(str);
}

/* ============================================================================
 * Test: Memory Functions
 * ============================================================================ */

TEST(memory_functions) {
    void *ptr;
    
    /* Allocation */
    ptr = md_alloc(10, sizeof(int));
    ASSERT_NOT_NULL(ptr);
    md_free(ptr);
    
    /* Zero-size allocation */
    ptr = md_alloc(0, sizeof(int));
    ASSERT_NULL(ptr);
    
    /* Realloc */
    ptr = md_alloc(10, sizeof(int));
    ASSERT_NOT_NULL(ptr);
    ptr = md_realloc(ptr, 20 * sizeof(int));
    ASSERT_NOT_NULL(ptr);
    md_free(ptr);
    
    /* String duplication */
    char *str = md_strdup("hello");
    ASSERT_NOT_NULL(str);
    ASSERT_EQ(strcmp(str, "hello"), 0);
    md_free(str);
    
    str = md_strndup("hello world", 5);
    ASSERT_NOT_NULL(str);
    ASSERT_EQ(strcmp(str, "hello"), 0);
    md_free(str);
}

/* ============================================================================
 * Test: Logger
 * ============================================================================ */

TEST(logger_init) {
    md_logger_config_t config;
    md_logger_config_init(&config);
    
    config.level = MD_LOG_DEBUG;
    config.destinations = MD_LOG_DEST_STDERR;
    config.options = MD_LOG_OPT_TIMESTAMP | MD_LOG_OPT_LEVEL;
    
    ASSERT_SUCCESS(md_logger_init(&config));
    ASSERT_TRUE(md_logger_is_initialized());
    
    MD_INFO("Logger test message");
    MD_DEBUG("Debug message");
    
    md_logger_shutdown();
    ASSERT_FALSE(md_logger_is_initialized());
}

TEST(logger_levels) {
    ASSERT_SUCCESS(md_logger_init(NULL));
    
    ASSERT_EQ(md_logger_get_level(), MD_LOG_INFO);
    
    ASSERT_SUCCESS(md_logger_set_level(MD_LOG_DEBUG));
    ASSERT_EQ(md_logger_get_level(), MD_LOG_DEBUG);
    
    ASSERT_TRUE(md_logger_would_log(MD_LOG_ERROR));
    ASSERT_TRUE(md_logger_would_log(MD_LOG_DEBUG));
    ASSERT_FALSE(md_logger_would_log(MD_LOG_TRACE));
    
    md_logger_shutdown();
}

/* ============================================================================
 * Test: I/O Buffer
 * ============================================================================ */

TEST(io_buffer_basic) {
    md_io_buffer_t *buf = md_io_buffer_create(1024);
    ASSERT_NOT_NULL(buf);
    
    ASSERT_EQ(md_io_buffer_size(buf), 0);
    ASSERT_EQ(md_io_buffer_capacity(buf), 1024);
    
    /* Write */
    const char *data = "Hello, World!";
    ssize_t written = md_io_buffer_write(buf, data, strlen(data));
    ASSERT_EQ(written, (ssize_t)strlen(data));
    ASSERT_EQ(md_io_buffer_size(buf), strlen(data));
    
    /* Peek */
    char peek_buf[32];
    ssize_t peeked = md_io_buffer_peek(buf, peek_buf, sizeof(peek_buf));
    ASSERT_EQ(peeked, (ssize_t)strlen(data));
    ASSERT_EQ(memcmp(peek_buf, data, strlen(data)), 0);
    ASSERT_EQ(md_io_buffer_size(buf), strlen(data));  /* Size unchanged */
    
    /* Read */
    char read_buf[32];
    ssize_t nread = md_io_buffer_read(buf, read_buf, sizeof(read_buf));
    ASSERT_EQ(nread, (ssize_t)strlen(data));
    ASSERT_EQ(memcmp(read_buf, data, strlen(data)), 0);
    ASSERT_EQ(md_io_buffer_size(buf), 0);
    
    md_io_buffer_destroy(buf);
}

TEST(io_buffer_line) {
    md_io_buffer_t *buf = md_io_buffer_create(256);
    ASSERT_NOT_NULL(buf);
    
    /* No line initially */
    size_t line_len;
    ASSERT_FALSE(md_io_buffer_has_line(buf, &line_len));
    
    /* Write data without newline */
    md_io_buffer_write(buf, "Hello", 5);
    ASSERT_FALSE(md_io_buffer_has_line(buf, &line_len));
    
    /* Add newline */
    md_io_buffer_write(buf, "\n", 1);
    ASSERT_TRUE(md_io_buffer_has_line(buf, &line_len));
    ASSERT_EQ(line_len, 6);
    
    /* Read line */
    char line[32];
    ssize_t nread = md_io_buffer_read_line(buf, line, sizeof(line));
    ASSERT_EQ(nread, 6);
    ASSERT_EQ(strcmp(line, "Hello\n"), 0);
    
    md_io_buffer_destroy(buf);
}

TEST(io_buffer_expand) {
    md_io_buffer_t *buf = md_io_buffer_create(16);
    ASSERT_NOT_NULL(buf);
    
    /* Write more than capacity */
    char data[256];
    memset(data, 'A', sizeof(data));
    
    ssize_t written = md_io_buffer_write(buf, data, sizeof(data));
    ASSERT_EQ(written, sizeof(data));
    ASSERT_TRUE(md_io_buffer_capacity(buf) >= sizeof(data));
    
    /* Read all */
    char read_buf[256];
    ssize_t nread = md_io_buffer_read(buf, read_buf, sizeof(read_buf));
    ASSERT_EQ(nread, sizeof(data));
    
    md_io_buffer_destroy(buf);
}

/* ============================================================================
 * Test: Non-blocking I/O
 * ============================================================================ */

TEST(pipe_operations) {
    int read_fd, write_fd;
    
    /* Create pipe */
    ASSERT_SUCCESS(md_pipe_create(&read_fd, &write_fd, true));
    ASSERT_TRUE(read_fd >= 0);
    ASSERT_TRUE(write_fd >= 0);
    ASSERT_TRUE(md_is_nonblocking(read_fd));
    ASSERT_TRUE(md_is_nonblocking(write_fd));
    
    /* Write */
    const char *msg = "test message";
    ssize_t written = md_nb_write(write_fd, msg, strlen(msg));
    ASSERT_EQ(written, (ssize_t)strlen(msg));
    
    /* Wait for data */
    ASSERT_EQ(md_wait_readable(read_fd, 1000), 1);
    
    /* Read */
    char buf[64];
    ssize_t nread = md_nb_read(read_fd, buf, sizeof(buf));
    ASSERT_EQ(nread, (ssize_t)strlen(msg));
    buf[nread] = '\0';
    ASSERT_EQ(strcmp(buf, msg), 0);
    
    /* Close */
    md_close_fd(&read_fd);
    md_close_fd(&write_fd);
    ASSERT_EQ(read_fd, -1);
    ASSERT_EQ(write_fd, -1);
}

TEST(poll_operations) {
    int read_fd, write_fd;
    ASSERT_SUCCESS(md_pipe_create(&read_fd, &write_fd, true));
    
    /* Poll for write (should be ready immediately) */
    md_poll_event_t events[1];
    events[0].fd = write_fd;
    events[0].events = MD_IO_EVENT_WRITE;
    
    int result = md_poll(events, 1, 0);
    ASSERT_EQ(result, 1);
    ASSERT_TRUE(events[0].revents & MD_IO_EVENT_WRITE);
    
    /* Write data */
    md_nb_write(write_fd, "x", 1);
    
    /* Poll for read */
    events[0].fd = read_fd;
    events[0].events = MD_IO_EVENT_READ;
    events[0].revents = 0;
    
    result = md_poll(events, 1, 100);
    ASSERT_EQ(result, 1);
    ASSERT_TRUE(events[0].revents & MD_IO_EVENT_READ);
    
    md_close_fd(&read_fd);
    md_close_fd(&write_fd);
}

/* ============================================================================
 * Test: Signal Handling
 * ============================================================================ */

TEST(signal_init) {
    ASSERT_FALSE(md_signal_is_initialized());
    ASSERT_SUCCESS(md_signal_init());
    ASSERT_TRUE(md_signal_is_initialized());
    
    /* Test signal names */
    const char *name = md_signal_name(SIGTERM);
    ASSERT_NOT_NULL(name);
    ASSERT_EQ(strcmp(name, "SIGTERM"), 0);
    
    int signo = md_signal_from_name("SIGINT");
    ASSERT_EQ(signo, SIGINT);
    
    signo = md_signal_from_name("TERM");
    ASSERT_EQ(signo, SIGTERM);
    
    md_signal_shutdown();
    ASSERT_FALSE(md_signal_is_initialized());
}

/* ============================================================================
 * Test: Session Manager
 * ============================================================================ */

TEST(manager_create) {
    md_session_manager_t *manager = md_manager_create(NULL);
    ASSERT_NOT_NULL(manager);
    
    ASSERT_EQ(md_manager_get_session_count(manager), 0);
    ASSERT_FALSE(md_manager_session_exists(manager, 1));
    
    md_manager_destroy(manager);
}

TEST(manager_config) {
    md_manager_config_t config;
    md_manager_config_init(&config);
    
    config.max_sessions = 5;
    config.io_timeout_ms = 1000;
    
    md_session_manager_t *manager = md_manager_create(&config);
    ASSERT_NOT_NULL(manager);
    
    md_manager_destroy(manager);
}

TEST(session_create_cat) {
    /* Initialize dependencies */
    ASSERT_SUCCESS(md_logger_init(NULL));
    ASSERT_SUCCESS(md_signal_init());
    
    /* Create manager */
    md_session_manager_t *manager = md_manager_create(NULL);
    ASSERT_NOT_NULL(manager);
    
    /* Create session using cat (echoes stdin to stdout) */
    md_session_config_t config;
    md_session_config_init(&config);
    
    /* Use /bin/cat as a simple test program */
    md_session_config_set_debugger(&config, "/bin/cat");
    
    md_session_t *session = md_session_create(manager, &config);
    ASSERT_NOT_NULL(session);
    
    /* Check session info */
    md_session_info_t info;
    ASSERT_SUCCESS(md_session_get_info(session, &info));
    
    ASSERT_TRUE(info.session_id > 0);
    ASSERT_TRUE(info.pid > 0);
    ASSERT_EQ(info.state, MD_SESSION_STATE_RUNNING);
    ASSERT_TRUE(info.stdin_fd >= 0);
    ASSERT_TRUE(info.stdout_fd >= 0);
    
    /* Test communication */
    const char *msg = "Hello, Debugger!\n";
    size_t bytes_sent;
    ASSERT_SUCCESS(md_session_send_string(session, msg, &bytes_sent));
    ASSERT_EQ(bytes_sent, strlen(msg));
    
    /* Wait for response */
    usleep(100000);  /* Give cat time to echo */
    
    char buf[256];
    size_t bytes_read;
    ASSERT_SUCCESS(md_session_receive(session, buf, sizeof(buf), &bytes_read));
    
    if (bytes_read > 0) {
        buf[bytes_read] = '\0';
        ASSERT_EQ(strcmp(buf, msg), 0);
    }
    
    /* Check statistics */
    md_session_stats_t stats;
    ASSERT_SUCCESS(md_session_get_stats(session, &stats));
    ASSERT_TRUE(stats.bytes_sent > 0);
    
    /* Destroy session */
    ASSERT_SUCCESS(md_session_destroy(session));
    
    /* Destroy manager */
    md_manager_destroy(manager);
    
    /* Cleanup */
    md_signal_shutdown();
    md_logger_shutdown();
}

TEST(session_multiple) {
    ASSERT_SUCCESS(md_logger_init(NULL));
    ASSERT_SUCCESS(md_signal_init());
    
    md_manager_config_t config;
    md_manager_config_init(&config);
    config.max_sessions = 3;
    
    md_session_manager_t *manager = md_manager_create(&config);
    ASSERT_NOT_NULL(manager);
    
    /* Create multiple sessions */
    md_session_t *sessions[3];
    
    for (int i = 0; i < 3; i++) {
        md_session_config_t sconfig;
        md_session_config_init(&sconfig);
        md_session_config_set_debugger(&sconfig, "/bin/cat");
        
        sessions[i] = md_session_create(manager, &sconfig);
        ASSERT_NOT_NULL(sessions[i]);
    }
    
    ASSERT_EQ(md_manager_get_session_count(manager), 3);
    
    /* Get session IDs */
    int ids[3];
    int count = md_manager_get_session_ids(manager, ids, 3);
    ASSERT_EQ(count, 3);
    
    /* Verify all sessions exist */
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(md_manager_session_exists(manager, ids[i]));
    }
    
    /* Destroy all sessions */
    for (int i = 0; i < 3; i++) {
        ASSERT_SUCCESS(md_session_destroy(sessions[i]));
    }
    
    ASSERT_EQ(md_manager_get_session_count(manager), 0);
    
    md_manager_destroy(manager);
    md_signal_shutdown();
    md_logger_shutdown();
}

/* ============================================================================
 * Test: Process Control
 * ============================================================================ */

TEST(session_terminate) {
    ASSERT_SUCCESS(md_logger_init(NULL));
    ASSERT_SUCCESS(md_signal_init());
    
    md_session_manager_t *manager = md_manager_create(NULL);
    ASSERT_NOT_NULL(manager);
    
    md_session_config_t config;
    md_session_config_init(&config);
    md_session_config_set_debugger(&config, "/bin/cat");
    
    md_session_t *session = md_session_create(manager, &config);
    ASSERT_NOT_NULL(session);
    
    ASSERT_TRUE(md_session_is_running(session));
    
    /* Terminate gracefully */
    ASSERT_SUCCESS(md_session_terminate(session, false));
    
    /* Wait for termination */
    int exit_code;
    ASSERT_SUCCESS(md_session_wait(session, 2000, &exit_code));
    
    ASSERT_FALSE(md_session_is_running(session));
    ASSERT_EQ(md_session_get_state(session), MD_SESSION_STATE_TERMINATED);
    
    md_session_destroy(session);
    md_manager_destroy(manager);
    md_signal_shutdown();
    md_logger_shutdown();
}

/* ============================================================================
 * Test: Debugger Detection
 * ============================================================================ */

TEST(debugger_detection) {
    /* Test debugger type detection */
    md_debugger_type_t type;
    
    type = md_detect_debugger_type("/usr/bin/lldb-dap");
    ASSERT_EQ(type, MD_DEBUGGER_LLDB);
    
    type = md_detect_debugger_type("/usr/bin/gdb");
    ASSERT_EQ(type, MD_DEBUGGER_GDB);
    
    type = md_detect_debugger_type("/usr/bin/bashdb");
    ASSERT_EQ(type, MD_DEBUGGER_SHELL);
    
    type = md_detect_debugger_type("/usr/bin/cat");
    ASSERT_EQ(type, MD_DEBUGGER_CUSTOM);
    
    /* Test debugger exists */
    ASSERT_TRUE(md_debugger_exists("/bin/cat"));
    ASSERT_FALSE(md_debugger_exists("/nonexistent/debugger"));
    
    /* Test find debugger */
    char path[MD_MAX_PATH_LEN];
    md_error_t err = md_find_debugger(MD_DEBUGGER_LLDB, path, sizeof(path));
    /* May or may not find it depending on system */
    (void)err;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    printf("\n");
    printf("========================================\n");
    printf(" Magic Debugger - Phase 1 Tests\n");
    printf("========================================\n\n");
    
    printf("[SUITE] Types and Utilities\n");
    RUN_TEST(error_strings);
    RUN_TEST(memory_functions);
    
    printf("\n[SUITE] Logger\n");
    RUN_TEST(logger_init);
    RUN_TEST(logger_levels);
    
    printf("\n[SUITE] I/O Buffer\n");
    RUN_TEST(io_buffer_basic);
    RUN_TEST(io_buffer_line);
    RUN_TEST(io_buffer_expand);
    
    printf("\n[SUITE] Non-blocking I/O\n");
    RUN_TEST(pipe_operations);
    RUN_TEST(poll_operations);
    
    printf("\n[SUITE] Signal Handling\n");
    RUN_TEST(signal_init);
    
    printf("\n[SUITE] Session Manager\n");
    RUN_TEST(manager_create);
    RUN_TEST(manager_config);
    RUN_TEST(session_create_cat);
    RUN_TEST(session_multiple);
    
    printf("\n[SUITE] Process Control\n");
    RUN_TEST(session_terminate);
    
    printf("\n[SUITE] Debugger Detection\n");
    RUN_TEST(debugger_detection);
    
    printf("\n========================================\n");
    printf(" All tests passed!\n");
    printf("========================================\n\n");
    
    return 0;
}
