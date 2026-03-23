/**
 * @file signal_handler.c
 * @brief Implementation of signal handling
 */

#include "signal_handler.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>  /* For strcasecmp */
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>    /* For fcntl, O_NONBLOCK, FD_CLOEXEC */
#include <sys/wait.h>
#include <pthread.h>

/* ============================================================================
 * Internal State
 * ============================================================================ */

/**
 * @brief Global signal handling state
 */
typedef struct {
    bool initialized;
    
    /* Self-pipe for signal-safe notification */
    int self_pipe_read;
    int self_pipe_write;
    
    /* Callbacks */
    md_child_exit_callback_t child_callback;
    void *child_callback_data;
    
    md_shutdown_callback_t shutdown_callback;
    void *shutdown_callback_data;
    
    /* State */
    volatile sig_atomic_t shutdown_requested;
    volatile sig_atomic_t child_status_changed;
    
    /* Saved signal actions */
    struct sigaction old_sigchld;
    struct sigaction old_sigterm;
    struct sigaction old_sigint;
    struct sigaction old_sigpipe;
    
} md_signal_state_t;

static md_signal_state_t g_signal_state = {
    .initialized = false,
    .self_pipe_read = -1,
    .self_pipe_write = -1,
    .shutdown_requested = 0,
    .child_status_changed = 0,
};

/* Signal name mapping */
static const struct {
    int signo;
    const char *name;
} g_signal_names[] = {
    { SIGABRT,   "SIGABRT" },
    { SIGALRM,   "SIGALRM" },
    { SIGBUS,    "SIGBUS" },
    { SIGCHLD,   "SIGCHLD" },
    { SIGCONT,   "SIGCONT" },
    { SIGFPE,    "SIGFPE" },
    { SIGHUP,    "SIGHUP" },
    { SIGILL,    "SIGILL" },
    { SIGINT,    "SIGINT" },
    { SIGIO,     "SIGIO" },
    { SIGKILL,   "SIGKILL" },
    { SIGPIPE,   "SIGPIPE" },
    { SIGPROF,   "SIGPROF" },
    { SIGQUIT,   "SIGQUIT" },
    { SIGSEGV,   "SIGSEGV" },
    { SIGSTOP,   "SIGSTOP" },
    { SIGSYS,    "SIGSYS" },
    { SIGTERM,   "SIGTERM" },
    { SIGTRAP,   "SIGTRAP" },
    { SIGTSTP,   "SIGTSTP" },
    { SIGTTIN,   "SIGTTIN" },
    { SIGTTOU,   "SIGTTOU" },
    { SIGURG,    "SIGURG" },
    { SIGUSR1,   "SIGUSR1" },
    { SIGUSR2,   "SIGUSR2" },
    { SIGVTALRM, "SIGVTALRM" },
    { SIGWINCH,  "SIGWINCH" },
    { SIGXCPU,   "SIGXCPU" },
    { SIGXFSZ,   "SIGXFSZ" },
};

/* ============================================================================
 * Signal Handlers (Async-signal-safe)
 * ============================================================================ */

/**
 * @brief Generic signal handler that writes to self-pipe
 * 
 * This function is async-signal-safe. It only writes a single byte
 * to the self-pipe to notify the main loop.
 */
static void signal_handler(int signo) {
    /* Set flags - these are async-signal-safe */
    switch (signo) {
        case SIGTERM:
        case SIGINT:
            g_signal_state.shutdown_requested = 1;
            break;
        case SIGCHLD:
            g_signal_state.child_status_changed = 1;
            break;
    }
    
    /* Write to self-pipe (async-signal-safe) */
    if (g_signal_state.self_pipe_write >= 0) {
        /* Write a single byte representing the signal */
        char c = (char)signo;
        ssize_t ignored __attribute__((unused));
        ignored = write(g_signal_state.self_pipe_write, &c, 1);
    }
}

/**
 * @brief SIGCHLD handler
 */
static void sigchld_handler(int signo) {
    signal_handler(signo);
}

/**
 * @brief SIGTERM/SIGINT handler
 */
static void shutdown_handler(int signo) {
    signal_handler(signo);
}

/**
 * @brief SIGPIPE handler (ignore)
 */
static void sigpipe_handler(int signo) {
    (void)signo;
    /* Ignore SIGPIPE - we handle EPIPE in write() */
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

md_error_t md_signal_init(void) {
    if (g_signal_state.initialized) {
        return MD_ERROR_ALREADY_INITIALIZED;
    }
    
    /* Create self-pipe */
    int pipe_fds[2];
    if (pipe(pipe_fds) < 0) {
        MD_ERROR("Failed to create self-pipe: %s", strerror(errno));
        return MD_ERROR_PIPE_FAILED;
    }
    
    g_signal_state.self_pipe_read = pipe_fds[0];
    g_signal_state.self_pipe_write = pipe_fds[1];
    
    /* Set non-blocking */
    fcntl(g_signal_state.self_pipe_read, F_SETFL, O_NONBLOCK);
    fcntl(g_signal_state.self_pipe_write, F_SETFL, O_NONBLOCK);
    
    /* Set close-on-exec */
    fcntl(g_signal_state.self_pipe_read, F_SETFD, FD_CLOEXEC);
    fcntl(g_signal_state.self_pipe_write, F_SETFD, FD_CLOEXEC);
    
    /* Install signal handlers */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    /* SIGCHLD */
    sa.sa_handler = sigchld_handler;
    sigaddset(&sa.sa_mask, SIGCHLD);
    if (sigaction(SIGCHLD, &sa, &g_signal_state.old_sigchld) < 0) {
        MD_ERROR("Failed to set SIGCHLD handler: %s", strerror(errno));
        close(g_signal_state.self_pipe_read);
        close(g_signal_state.self_pipe_write);
        return MD_ERROR_UNKNOWN;
    }
    
    /* SIGTERM */
    sa.sa_handler = shutdown_handler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGTERM);
    sigaddset(&sa.sa_mask, SIGINT);
    if (sigaction(SIGTERM, &sa, &g_signal_state.old_sigterm) < 0) {
        MD_ERROR("Failed to set SIGTERM handler: %s", strerror(errno));
        sigaction(SIGCHLD, &g_signal_state.old_sigchld, NULL);
        close(g_signal_state.self_pipe_read);
        close(g_signal_state.self_pipe_write);
        return MD_ERROR_UNKNOWN;
    }
    
    /* SIGINT */
    if (sigaction(SIGINT, &sa, &g_signal_state.old_sigint) < 0) {
        MD_ERROR("Failed to set SIGINT handler: %s", strerror(errno));
        sigaction(SIGTERM, &g_signal_state.old_sigterm, NULL);
        sigaction(SIGCHLD, &g_signal_state.old_sigchld, NULL);
        close(g_signal_state.self_pipe_read);
        close(g_signal_state.self_pipe_write);
        return MD_ERROR_UNKNOWN;
    }
    
    /* SIGPIPE - ignore */
    sa.sa_handler = sigpipe_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGPIPE, &sa, &g_signal_state.old_sigpipe) < 0) {
        MD_ERROR("Failed to set SIGPIPE handler: %s", strerror(errno));
        /* Continue anyway - not critical */
    }
    
    g_signal_state.initialized = true;
    g_signal_state.shutdown_requested = 0;
    g_signal_state.child_status_changed = 0;
    
    MD_DEBUG("Signal handling initialized");
    return MD_SUCCESS;
}

void md_signal_shutdown(void) {
    if (!g_signal_state.initialized) {
        return;
    }
    
    /* Restore original signal handlers */
    sigaction(SIGCHLD, &g_signal_state.old_sigchld, NULL);
    sigaction(SIGTERM, &g_signal_state.old_sigterm, NULL);
    sigaction(SIGINT, &g_signal_state.old_sigint, NULL);
    sigaction(SIGPIPE, &g_signal_state.old_sigpipe, NULL);
    
    /* Close self-pipe */
    if (g_signal_state.self_pipe_read >= 0) {
        close(g_signal_state.self_pipe_read);
        g_signal_state.self_pipe_read = -1;
    }
    if (g_signal_state.self_pipe_write >= 0) {
        close(g_signal_state.self_pipe_write);
        g_signal_state.self_pipe_write = -1;
    }
    
    g_signal_state.initialized = false;
    MD_DEBUG("Signal handling shutdown");
}

bool md_signal_is_initialized(void) {
    return g_signal_state.initialized;
}

/* ============================================================================
 * Self-pipe Operations
 * ============================================================================ */

md_error_t md_signal_create_self_pipe(int *read_fd, int *write_fd) {
    if (read_fd == NULL || write_fd == NULL) {
        return MD_ERROR_NULL_POINTER;
    }
    
    int fds[2];
    if (pipe(fds) < 0) {
        return MD_ERROR_PIPE_FAILED;
    }
    
    *read_fd = fds[0];
    *write_fd = fds[1];
    
    fcntl(*read_fd, F_SETFL, O_NONBLOCK);
    fcntl(*write_fd, F_SETFL, O_NONBLOCK);
    
    return MD_SUCCESS;
}

int md_signal_notify(int write_fd) {
    char c = 1;
    return (int)write(write_fd, &c, 1);
}

int md_signal_clear_notification(int read_fd) {
    char buf[32];
    return (int)read(read_fd, buf, sizeof(buf));
}

/* ============================================================================
 * SIGCHLD Handling
 * ============================================================================ */

md_error_t md_signal_set_child_callback(md_child_exit_callback_t callback,
                                         void *user_data) {
    g_signal_state.child_callback = callback;
    g_signal_state.child_callback_data = user_data;
    return MD_SUCCESS;
}

int md_signal_waitpid(int pid, int *status, int options) {
    int stat;
    int result = waitpid(pid, &stat, options);
    
    if (status != NULL) {
        *status = stat;
    }
    
    return result;
}

bool md_signal_child_exited(int pid, int *exit_code) {
    int status;
    int result = waitpid(pid, &status, WNOHANG);
    
    if (result == 0) {
        /* Still running */
        return false;
    }
    
    if (result < 0) {
        /* Error or no such process */
        return false;
    }
    
    if (exit_code != NULL) {
        if (WIFEXITED(status)) {
            *exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            *exit_code = 128 + WTERMSIG(status);
        } else {
            *exit_code = -1;
        }
    }
    
    return true;
}

int md_signal_get_exit_code(int status) {
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

bool md_signal_was_signaled(int status) {
    return WIFSIGNALED(status) != 0;
}

int md_signal_get_term_signal(int status) {
    if (WIFSIGNALED(status)) {
        return WTERMSIG(status);
    }
    return 0;
}

bool md_signal_was_stopped(int status) {
    return WIFSTOPPED(status) != 0;
}

int md_signal_get_stop_signal(int status) {
    if (WIFSTOPPED(status)) {
        return WSTOPSIG(status);
    }
    return 0;
}

/* ============================================================================
 * Signal Masking
 * ============================================================================ */

md_error_t md_signal_block(int signo) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, signo);
    
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        return MD_ERROR_UNKNOWN;
    }
    
    return MD_SUCCESS;
}

md_error_t md_signal_unblock(int signo) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, signo);
    
    if (pthread_sigmask(SIG_UNBLOCK, &set, NULL) != 0) {
        return MD_ERROR_UNKNOWN;
    }
    
    return MD_SUCCESS;
}

md_error_t md_signal_block_multiple(const int *signals, int count) {
    if (signals == NULL || count <= 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    sigset_t set;
    sigemptyset(&set);
    
    for (int i = 0; i < count; i++) {
        sigaddset(&set, signals[i]);
    }
    
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        return MD_ERROR_UNKNOWN;
    }
    
    return MD_SUCCESS;
}

md_error_t md_signal_unblock_multiple(const int *signals, int count) {
    if (signals == NULL || count <= 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    sigset_t set;
    sigemptyset(&set);
    
    for (int i = 0; i < count; i++) {
        sigaddset(&set, signals[i]);
    }
    
    if (pthread_sigmask(SIG_UNBLOCK, &set, NULL) != 0) {
        return MD_ERROR_UNKNOWN;
    }
    
    return MD_SUCCESS;
}

md_error_t md_signal_save_and_block(const int *signals, int count, void *old_mask) {
    if (signals == NULL || count <= 0 || old_mask == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    sigset_t set;
    sigemptyset(&set);
    
    for (int i = 0; i < count; i++) {
        sigaddset(&set, signals[i]);
    }
    
    if (pthread_sigmask(SIG_BLOCK, &set, (sigset_t*)old_mask) != 0) {
        return MD_ERROR_UNKNOWN;
    }
    
    return MD_SUCCESS;
}

md_error_t md_signal_restore_mask(void *old_mask) {
    if (old_mask == NULL) {
        return MD_ERROR_NULL_POINTER;
    }
    
    if (pthread_sigmask(SIG_SETMASK, (sigset_t*)old_mask, NULL) != 0) {
        return MD_ERROR_UNKNOWN;
    }
    
    return MD_SUCCESS;
}

/* ============================================================================
 * Graceful Shutdown
 * ============================================================================ */

md_error_t md_signal_set_shutdown_callback(md_shutdown_callback_t callback,
                                            void *user_data) {
    g_signal_state.shutdown_callback = callback;
    g_signal_state.shutdown_callback_data = user_data;
    return MD_SUCCESS;
}

bool md_signal_shutdown_requested(void) {
    return g_signal_state.shutdown_requested != 0;
}

void md_signal_clear_shutdown_request(void) {
    g_signal_state.shutdown_requested = 0;
}

md_error_t md_signal_request_shutdown(void) {
    g_signal_state.shutdown_requested = 1;
    
    /* Invoke callback if set */
    if (g_signal_state.shutdown_callback != NULL) {
        g_signal_state.shutdown_callback(g_signal_state.shutdown_callback_data);
    }
    
    return MD_SUCCESS;
}

/* ============================================================================
 * Signal Information
 * ============================================================================ */

const char* md_signal_name(int signo) {
    for (size_t i = 0; i < sizeof(g_signal_names) / sizeof(g_signal_names[0]); i++) {
        if (g_signal_names[i].signo == signo) {
            return g_signal_names[i].name;
        }
    }
    return NULL;
}

int md_signal_from_name(const char *name) {
    if (name == NULL) {
        return -1;
    }
    
    /* Skip "SIG" prefix if present */
    const char *signame = name;
    if (strncmp(name, "SIG", 3) == 0) {
        signame = name + 3;
    }
    
    for (size_t i = 0; i < sizeof(g_signal_names) / sizeof(g_signal_names[0]); i++) {
        const char *full_name = g_signal_names[i].name;
        if (strcasecmp(name, full_name) == 0 ||
            strcasecmp(signame, full_name + 3) == 0) {
            return g_signal_names[i].signo;
        }
    }
    
    return -1;
}

bool md_signal_is_valid(int signo) {
    return md_signal_name(signo) != NULL;
}

/* ============================================================================
 * Process Termination
 * ============================================================================ */

md_error_t md_signal_kill(int pid, int signo) {
    if (kill(pid, signo) < 0) {
        switch (errno) {
            case ESRCH:
                return MD_ERROR_SESSION_NOT_FOUND;
            case EPERM:
                return MD_ERROR_PERMISSION;
            default:
                return MD_ERROR_UNKNOWN;
        }
    }
    return MD_SUCCESS;
}

md_error_t md_signal_terminate(int pid) {
    return md_signal_kill(pid, SIGTERM);
}

md_error_t md_signal_force_kill(int pid) {
    return md_signal_kill(pid, SIGKILL);
}

md_error_t md_signal_interrupt(int pid) {
    return md_signal_kill(pid, SIGINT);
}

md_error_t md_signal_continue(int pid) {
    return md_signal_kill(pid, SIGCONT);
}

md_error_t md_signal_stop(int pid) {
    return md_signal_kill(pid, SIGSTOP);
}
