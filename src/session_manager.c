/**
 * @file session_manager.c
 * @brief Implementation of Session Manager
 * 
 * The Session Manager is responsible for:
 * - Managing debug session lifecycle
 * - Process creation (fork + exec)
 * - Pipe communication setup
 * - Non-blocking I/O management
 */

#include "session_manager.h"
#include "io_handler.h"
#include "signal_handler.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>
#include <time.h>

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal debug session structure
 */
struct md_session {
    int id;                                 /**< Session ID */
    pid_t pid;                              /**< Debugger process ID */
    
    int stdin_fd;                           /**< FD for writing to debugger */
    int stdout_fd;                          /**< FD for reading from debugger */
    int stderr_fd;                          /**< FD for reading stderr */
    
    md_session_state_t state;               /**< Current state */
    md_debugger_type_t debugger_type;       /**< Debugger type */
    char debugger_name[MD_MAX_DEBUGGER_NAME]; /**< Debugger name */
    char program_path[MD_MAX_PATH_LEN];     /**< Program being debugged */
    
    int exit_code;                          /**< Exit code (if terminated) */
    bool signaled;                          /**< Terminated by signal */
    int term_signal;                        /**< Terminating signal */
    
    /* Statistics */
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t messages_sent;
    uint64_t messages_received;
    time_t start_time;
    int restart_count;
    
    /* I/O buffers */
    md_io_buffer_t *read_buffer;
    md_io_buffer_t *write_buffer;
    
    /* Manager reference */
    struct md_session_manager *manager;
    
    /* Next in list */
    struct md_session *next;
};

/**
 * @brief Session manager structure
 */
struct md_session_manager {
    int max_sessions;
    int session_count;
    int next_session_id;
    
    md_session_t *sessions;                 /**< Linked list of sessions */
    md_session_t **session_slots;           /**< Array of pointers for quick lookup */
    
    int io_timeout_ms;
    bool auto_cleanup;
    
    md_exit_callback_t exit_callback;
    void *callback_data;
    
    pthread_mutex_t mutex;
    bool initialized;
};

/* ============================================================================
 * Debugger Detection
 * ============================================================================ */

/* Common debugger paths */
static const char *g_lldb_paths[] = {
    "/usr/bin/lldb-dap",
    "/usr/local/bin/lldb-dap",
    "/opt/homebrew/bin/lldb-dap",
    NULL
};

static const char *g_gdb_paths[] = {
    "/usr/bin/gdb",
    "/usr/local/bin/gdb",
    "/opt/homebrew/bin/gdb",
    NULL
};

static const char *g_shell_debugger_paths[] = {
    "/usr/bin/bashdb",
    "/usr/local/bin/bashdb",
    NULL
};

md_error_t md_find_debugger(md_debugger_type_t type, char *path, size_t size) {
    if (path == NULL || size == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    const char **paths = NULL;
    
    switch (type) {
        case MD_DEBUGGER_LLDB:
            paths = g_lldb_paths;
            break;
        case MD_DEBUGGER_GDB:
            paths = g_gdb_paths;
            break;
        case MD_DEBUGGER_SHELL:
            paths = g_shell_debugger_paths;
            break;
        default:
            return MD_ERROR_INVALID_ARG;
    }
    
    for (int i = 0; paths[i] != NULL; i++) {
        if (access(paths[i], X_OK) == 0) {
            strncpy(path, paths[i], size - 1);
            path[size - 1] = '\0';
            return MD_SUCCESS;
        }
    }
    
    return MD_ERROR_SESSION_NOT_FOUND;
}

md_debugger_type_t md_detect_debugger_type(const char *path) {
    if (path == NULL) {
        return MD_DEBUGGER_NONE;
    }
    
    /* Get basename */
    const char *basename = strrchr(path, '/');
    basename = basename ? basename + 1 : path;
    
    if (strstr(basename, "lldb") != NULL || strstr(basename, "llvm") != NULL) {
        return MD_DEBUGGER_LLDB;
    }
    if (strstr(basename, "gdb") != NULL) {
        return MD_DEBUGGER_GDB;
    }
    if (strstr(basename, "bashdb") != NULL || strstr(basename, "shdb") != NULL) {
        return MD_DEBUGGER_SHELL;
    }
    
    return MD_DEBUGGER_CUSTOM;
}

bool md_debugger_exists(const char *path) {
    if (path == NULL) {
        return false;
    }
    return access(path, X_OK) == 0;
}

/* ============================================================================
 * Configuration Helpers
 * ============================================================================ */

void md_session_config_init(md_session_config_t *config) {
    if (config == NULL) {
        return;
    }
    
    memset(config, 0, sizeof(md_session_config_t));
    config->debugger_type = MD_DEBUGGER_NONE;
    config->redirect_stderr = true;
    config->create_pty = false;
}

md_error_t md_session_config_set_debugger(md_session_config_t *config, const char *path) {
    if (config == NULL || path == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    strncpy(config->debugger_path, path, MD_MAX_PATH_LEN - 1);
    config->debugger_path[MD_MAX_PATH_LEN - 1] = '\0';
    config->debugger_type = md_detect_debugger_type(path);
    
    return MD_SUCCESS;
}

md_error_t md_session_config_set_program(md_session_config_t *config, const char *path) {
    if (config == NULL || path == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    strncpy(config->program_path, path, MD_MAX_PATH_LEN - 1);
    config->program_path[MD_MAX_PATH_LEN - 1] = '\0';
    
    return MD_SUCCESS;
}

md_error_t md_session_config_add_arg(md_session_config_t *config, const char *arg) {
    if (config == NULL || arg == NULL) {
        return MD_ERROR_INVALID_ARG;
    }
    
    if (config->arg_count >= MD_MAX_ARGS) {
        return MD_ERROR_BUFFER_OVERFLOW;
    }
    
    config->args[config->arg_count] = md_strdup(arg);
    if (config->args[config->arg_count] == NULL) {
        return MD_ERROR_OUT_OF_MEMORY;
    }
    
    config->arg_count++;
    return MD_SUCCESS;
}

void md_session_config_clear_args(md_session_config_t *config) {
    if (config == NULL) {
        return;
    }
    
    for (int i = 0; i < config->arg_count; i++) {
        MD_SAFE_FREE(config->args[i]);
    }
    config->arg_count = 0;
}

void md_manager_config_init(md_manager_config_t *config) {
    if (config == NULL) {
        return;
    }
    
    memset(config, 0, sizeof(md_manager_config_t));
    config->max_sessions = MD_MAX_SESSIONS;
    config->io_timeout_ms = MD_DEFAULT_TIMEOUT_MS;
    config->auto_cleanup = true;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* md_session_state_string(md_session_state_t state) {
    static const char *state_names[] = {
        "NONE",
        "STARTING",
        "RUNNING",
        "STOPPED",
        "EXITING",
        "TERMINATED",
    };
    
    if (state < MD_SESSION_STATE_NONE || state > MD_SESSION_STATE_TERMINATED) {
        return "UNKNOWN";
    }
    
    return state_names[state];
}

const char* md_debugger_type_string(md_debugger_type_t type) {
    static const char *type_names[] = {
        "NONE",
        "LLDB",
        "GDB",
        "SHELL",
    };
    
    if (type < MD_DEBUGGER_NONE || type > MD_DEBUGGER_SHELL) {
        if (type == MD_DEBUGGER_CUSTOM) {
            return "CUSTOM";
        }
        return "UNKNOWN";
    }
    
    return type_names[type];
}

/* ============================================================================
 * Session Manager API
 * ============================================================================ */

md_session_manager_t* md_manager_create(const md_manager_config_t *config) {
    md_session_manager_t *manager = (md_session_manager_t*)calloc(1, sizeof(md_session_manager_t));
    if (manager == NULL) {
        return NULL;
    }
    
    /* Apply configuration */
    if (config != NULL) {
        manager->max_sessions = config->max_sessions > 0 ? config->max_sessions : MD_MAX_SESSIONS;
        manager->io_timeout_ms = config->io_timeout_ms > 0 ? config->io_timeout_ms : MD_DEFAULT_TIMEOUT_MS;
        manager->auto_cleanup = config->auto_cleanup;
        manager->exit_callback = config->on_exit;
        manager->callback_data = config->callback_data;
    } else {
        manager->max_sessions = MD_MAX_SESSIONS;
        manager->io_timeout_ms = MD_DEFAULT_TIMEOUT_MS;
        manager->auto_cleanup = true;
    }
    
    /* Allocate session slots */
    manager->session_slots = (md_session_t**)calloc(manager->max_sessions, sizeof(md_session_t*));
    if (manager->session_slots == NULL) {
        free(manager);
        return NULL;
    }
    
    /* Initialize mutex */
    if (pthread_mutex_init(&manager->mutex, NULL) != 0) {
        free(manager->session_slots);
        free(manager);
        return NULL;
    }
    
    manager->next_session_id = 1;
    manager->initialized = true;
    
    MD_INFO("Session manager created (max_sessions=%d)", manager->max_sessions);
    return manager;
}

void md_manager_destroy(md_session_manager_t *manager) {
    if (manager == NULL) {
        return;
    }
    
    pthread_mutex_lock(&manager->mutex);
    
    /* Terminate all sessions */
    md_session_t *session = manager->sessions;
    while (session != NULL) {
        md_session_t *next = session->next;
        
        /* Mark as exiting */
        session->state = MD_SESSION_STATE_EXITING;
        
        /* Terminate the process */
        if (session->pid > 0) {
            kill(session->pid, SIGTERM);
            
            /* Give it a moment to exit gracefully */
            usleep(100000);
            
            /* Check if still running */
            if (kill(session->pid, 0) == 0) {
                /* Force kill */
                kill(session->pid, SIGKILL);
            }
        }
        
        /* Close file descriptors */
        md_close_fd(&session->stdin_fd);
        md_close_fd(&session->stdout_fd);
        md_close_fd(&session->stderr_fd);
        
        /* Free buffers */
        if (session->read_buffer != NULL) {
            md_io_buffer_destroy(session->read_buffer);
        }
        if (session->write_buffer != NULL) {
            md_io_buffer_destroy(session->write_buffer);
        }
        
        free(session);
        session = next;
    }
    
    manager->sessions = NULL;
    manager->session_count = 0;
    manager->initialized = false;
    
    pthread_mutex_unlock(&manager->mutex);
    pthread_mutex_destroy(&manager->mutex);
    
    free(manager->session_slots);
    free(manager);
    
    MD_INFO("Session manager destroyed");
}

int md_manager_get_session_count(md_session_manager_t *manager) {
    if (manager == NULL) {
        return -1;
    }
    
    pthread_mutex_lock(&manager->mutex);
    int count = manager->session_count;
    pthread_mutex_unlock(&manager->mutex);
    
    return count;
}

int md_manager_get_session_ids(md_session_manager_t *manager, int *ids, int max_count) {
    if (manager == NULL || ids == NULL || max_count <= 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    pthread_mutex_lock(&manager->mutex);
    
    int count = 0;
    md_session_t *session = manager->sessions;
    
    while (session != NULL && count < max_count) {
        ids[count++] = session->id;
        session = session->next;
    }
    
    pthread_mutex_unlock(&manager->mutex);
    
    return count;
}

bool md_manager_session_exists(md_session_manager_t *manager, int session_id) {
    if (manager == NULL || session_id <= 0) {
        return false;
    }
    
    pthread_mutex_lock(&manager->mutex);
    
    md_session_t *session = manager->sessions;
    while (session != NULL) {
        if (session->id == session_id) {
            pthread_mutex_unlock(&manager->mutex);
            return true;
        }
        session = session->next;
    }
    
    pthread_mutex_unlock(&manager->mutex);
    return false;
}

/* ============================================================================
 * Session Creation
 * ============================================================================ */

md_session_t* md_session_create(md_session_manager_t *manager, const md_session_config_t *config) {
    if (manager == NULL || config == NULL) {
        MD_ERROR("Invalid arguments: manager=%p, config=%p", manager, config);
        return NULL;
    }
    
    if (config->debugger_path[0] == '\0') {
        MD_ERROR("No debugger path specified");
        return NULL;
    }
    
    pthread_mutex_lock(&manager->mutex);
    
    /* Check session limit */
    if (manager->session_count >= manager->max_sessions) {
        MD_ERROR("Maximum session limit reached (%d)", manager->max_sessions);
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }
    
    /* Allocate session */
    md_session_t *session = (md_session_t*)calloc(1, sizeof(md_session_t));
    if (session == NULL) {
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }
    
    /* Initialize session */
    session->id = manager->next_session_id++;
    session->state = MD_SESSION_STATE_STARTING;
    session->debugger_type = config->debugger_type;
    session->manager = manager;
    session->stdin_fd = -1;
    session->stdout_fd = -1;
    session->stderr_fd = -1;
    session->start_time = time(NULL);
    
    /* Copy configuration */
    strncpy(session->program_path, config->program_path, MD_MAX_PATH_LEN - 1);
    
    /* Extract debugger name */
    const char *basename = strrchr(config->debugger_path, '/');
    basename = basename ? basename + 1 : config->debugger_path;
    strncpy(session->debugger_name, basename, MD_MAX_DEBUGGER_NAME - 1);
    
    /* Create I/O buffers */
    session->read_buffer = md_io_buffer_create(MD_IO_BUFFER_SIZE);
    session->write_buffer = md_io_buffer_create(MD_IO_BUFFER_SIZE);
    
    if (session->read_buffer == NULL || session->write_buffer == NULL) {
        MD_ERROR("Failed to create I/O buffers");
        md_io_buffer_destroy(session->read_buffer);
        md_io_buffer_destroy(session->write_buffer);
        free(session);
        pthread_mutex_unlock(&manager->mutex);
        return NULL;
    }
    
    /* Create pipes */
    int stdin_pipe[2] = { -1, -1 };
    int stdout_pipe[2] = { -1, -1 };
    int stderr_pipe[2] = { -1, -1 };
    
    /* stdin pipe: parent writes, child reads */
    if (md_pipe_create_cloexec(&stdin_pipe[0], &stdin_pipe[1], false) != MD_SUCCESS) {
        MD_ERROR("Failed to create stdin pipe");
        goto cleanup_error;
    }
    
    /* stdout pipe: child writes, parent reads */
    if (md_pipe_create_cloexec(&stdout_pipe[0], &stdout_pipe[1], false) != MD_SUCCESS) {
        MD_ERROR("Failed to create stdout pipe");
        goto cleanup_error;
    }
    
    /* stderr pipe (optional) */
    if (config->redirect_stderr) {
        if (md_pipe_create_cloexec(&stderr_pipe[0], &stderr_pipe[1], false) != MD_SUCCESS) {
            MD_ERROR("Failed to create stderr pipe");
            goto cleanup_error;
        }
    }
    
    MD_DEBUG("Pipes created: stdin=[%d,%d], stdout=[%d,%d], stderr=[%d,%d]",
             stdin_pipe[0], stdin_pipe[1],
             stdout_pipe[0], stdout_pipe[1],
             stderr_pipe[0], stderr_pipe[1]);
    
    /* Fork */
    pid_t pid = fork();
    
    if (pid < 0) {
        MD_ERROR("Fork failed: %s", strerror(errno));
        goto cleanup_error;
    }
    
    if (pid == 0) {
        /* Child process */
        
        /* Redirect stdin */
        if (dup2(stdin_pipe[0], STDIN_FILENO) < 0) {
            _exit(127);
        }
        
        /* Redirect stdout */
        if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0) {
            _exit(127);
        }
        
        /* Redirect stderr */
        if (config->redirect_stderr) {
            if (dup2(stderr_pipe[1], STDERR_FILENO) < 0) {
                _exit(127);
            }
        }
        
        /* Close all pipe ends */
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        if (config->redirect_stderr) {
            close(stderr_pipe[0]);
            close(stderr_pipe[1]);
        }
        
        /* Change working directory if specified */
        if (config->working_dir[0] != '\0') {
            if (chdir(config->working_dir) < 0) {
                _exit(127);
            }
        }
        
        /* Build argument list */
        char *argv[MD_MAX_ARGS + 4];
        int argc = 0;
        
        argv[argc++] = (char*)config->debugger_path;
        
        /* Add program path as argument */
        if (config->program_path[0] != '\0') {
            argv[argc++] = (char*)config->program_path;
        }
        
        /* Add extra arguments */
        for (int i = 0; i < config->arg_count && argc < MD_MAX_ARGS + 2; i++) {
            argv[argc++] = config->args[i];
        }
        
        argv[argc] = NULL;
        
        /* Execute debugger */
        execvp(config->debugger_path, argv);
        
        /* If we get here, exec failed */
        _exit(127);
    }
    
    /* Parent process */
    
    /* Close child ends of pipes */
    close(stdin_pipe[0]);   /* Read end - child uses this */
    close(stdout_pipe[1]);  /* Write end - child uses this */
    if (config->redirect_stderr) {
        close(stderr_pipe[1]);  /* Write end - child uses this */
    }
    
    /* Store file descriptors */
    session->stdin_fd = stdin_pipe[1];   /* Write to debugger */
    session->stdout_fd = stdout_pipe[0]; /* Read from debugger */
    session->stderr_fd = config->redirect_stderr ? stderr_pipe[0] : -1;
    session->pid = pid;
    
    /* Set non-blocking */
    md_set_nonblocking(session->stdin_fd);
    md_set_nonblocking(session->stdout_fd);
    if (session->stderr_fd >= 0) {
        md_set_nonblocking(session->stderr_fd);
    }
    
    /* Update state */
    session->state = MD_SESSION_STATE_RUNNING;
    
    /* Add to manager */
    session->next = manager->sessions;
    manager->sessions = session;
    manager->session_count++;
    
    pthread_mutex_unlock(&manager->mutex);
    
    MD_INFO("Session %d created: pid=%d, debugger=%s",
            session->id, session->pid, session->debugger_name);
    
    return session;
    
cleanup_error:
    /* Clean up pipes */
    if (stdin_pipe[0] >= 0) close(stdin_pipe[0]);
    if (stdin_pipe[1] >= 0) close(stdin_pipe[1]);
    if (stdout_pipe[0] >= 0) close(stdout_pipe[0]);
    if (stdout_pipe[1] >= 0) close(stdout_pipe[1]);
    if (stderr_pipe[0] >= 0) close(stderr_pipe[0]);
    if (stderr_pipe[1] >= 0) close(stderr_pipe[1]);
    
    md_io_buffer_destroy(session->read_buffer);
    md_io_buffer_destroy(session->write_buffer);
    free(session);
    
    pthread_mutex_unlock(&manager->mutex);
    return NULL;
}

md_error_t md_session_destroy(md_session_t *session) {
    if (session == NULL) {
        return MD_ERROR_NULL_POINTER;
    }
    
    md_session_manager_t *manager = session->manager;
    
    pthread_mutex_lock(&manager->mutex);
    
    /* Update state */
    session->state = MD_SESSION_STATE_EXITING;
    
    /* Terminate process if running */
    if (session->pid > 0 && kill(session->pid, 0) == 0) {
        /* Try graceful termination first */
        kill(session->pid, SIGTERM);
        
        /* Wait briefly */
        for (int i = 0; i < 10; i++) {
            if (kill(session->pid, 0) != 0) {
                break;
            }
            usleep(100000);
        }
        
        /* Force kill if still running */
        if (kill(session->pid, 0) == 0) {
            MD_WARNING("Force killing session %d (pid=%d)", session->id, session->pid);
            kill(session->pid, SIGKILL);
        }
    }
    
    /* Close file descriptors */
    md_close_fd(&session->stdin_fd);
    md_close_fd(&session->stdout_fd);
    md_close_fd(&session->stderr_fd);
    
    /* Free buffers */
    md_io_buffer_destroy(session->read_buffer);
    md_io_buffer_destroy(session->write_buffer);
    
    /* Remove from manager */
    md_session_t **ptr = &manager->sessions;
    while (*ptr != NULL) {
        if (*ptr == session) {
            *ptr = session->next;
            break;
        }
        ptr = &(*ptr)->next;
    }
    
    manager->session_count--;
    
    MD_INFO("Session %d destroyed", session->id);
    
    free(session);
    
    pthread_mutex_unlock(&manager->mutex);
    
    return MD_SUCCESS;
}

/* ============================================================================
 * Session Information
 * ============================================================================ */

int md_session_get_id(md_session_t *session) {
    if (session == NULL) {
        return -1;
    }
    return session->id;
}

md_error_t md_session_get_info(md_session_t *session, md_session_info_t *info) {
    if (session == NULL || info == NULL) {
        return MD_ERROR_NULL_POINTER;
    }
    
    pthread_mutex_lock(&session->manager->mutex);
    
    info->session_id = session->id;
    info->pid = session->pid;
    info->stdin_fd = session->stdin_fd;
    info->stdout_fd = session->stdout_fd;
    info->stderr_fd = session->stderr_fd;
    info->state = session->state;
    info->debugger_type = session->debugger_type;
    strncpy(info->debugger_name, session->debugger_name, MD_MAX_DEBUGGER_NAME - 1);
    info->debugger_name[MD_MAX_DEBUGGER_NAME - 1] = '\0';
    strncpy(info->program_path, session->program_path, MD_MAX_PATH_LEN - 1);
    info->program_path[MD_MAX_PATH_LEN - 1] = '\0';
    info->exit_code = session->exit_code;
    info->is_valid = true;
    
    pthread_mutex_unlock(&session->manager->mutex);
    
    return MD_SUCCESS;
}

md_error_t md_session_get_stats(md_session_t *session, md_session_stats_t *stats) {
    if (session == NULL || stats == NULL) {
        return MD_ERROR_NULL_POINTER;
    }
    
    pthread_mutex_lock(&session->manager->mutex);
    
    stats->bytes_sent = session->bytes_sent;
    stats->bytes_received = session->bytes_received;
    stats->messages_sent = session->messages_sent;
    stats->messages_received = session->messages_received;
    stats->uptime_ms = (uint64_t)(time(NULL) - session->start_time) * 1000;
    stats->restart_count = session->restart_count;
    
    pthread_mutex_unlock(&session->manager->mutex);
    
    return MD_SUCCESS;
}

md_debugger_type_t md_session_get_debugger_type(md_session_t *session) {
    if (session == NULL) {
        return MD_DEBUGGER_NONE;
    }
    return session->debugger_type;
}

/* ============================================================================
 * Process Control
 * ============================================================================ */

md_error_t md_session_terminate(md_session_t *session, bool force) {
    if (session == NULL) {
        return MD_ERROR_NULL_POINTER;
    }
    
    if (session->pid <= 0) {
        return MD_ERROR_SESSION_NOT_FOUND;
    }
    
    int sig = force ? SIGKILL : SIGTERM;
    
    MD_DEBUG("Sending %s to session %d (pid=%d)",
             force ? "SIGKILL" : "SIGTERM", session->id, session->pid);
    
    if (kill(session->pid, sig) < 0) {
        if (errno == ESRCH) {
            session->state = MD_SESSION_STATE_TERMINATED;
            return MD_ERROR_PROCESS_EXITED;
        }
        return MD_ERROR_UNKNOWN;
    }
    
    if (!force) {
        session->state = MD_SESSION_STATE_EXITING;
    }
    
    return MD_SUCCESS;
}

md_error_t md_session_wait(md_session_t *session, int timeout_ms, int *exit_code) {
    if (session == NULL) {
        return MD_ERROR_NULL_POINTER;
    }
    
    if (session->pid <= 0) {
        return MD_ERROR_SESSION_NOT_FOUND;
    }
    
    int status;
    int options = 0;
    
    if (timeout_ms == 0) {
        options = WNOHANG;
    }
    
    /* Wait for process */
    int result = waitpid(session->pid, &status, options);
    
    if (result == 0) {
        /* Still running, need to wait with timeout */
        if (timeout_ms < 0) {
            /* Wait indefinitely */
            result = waitpid(session->pid, &status, 0);
        } else if (timeout_ms > 0) {
            /* Poll with timeout */
            int elapsed = 0;
            while (elapsed < timeout_ms) {
                result = waitpid(session->pid, &status, WNOHANG);
                if (result != 0) {
                    break;
                }
                usleep(10000);
                elapsed += 10;
            }
            
            if (result == 0) {
                return MD_ERROR_TIMEOUT;
            }
        }
    }
    
    if (result < 0) {
        if (errno == ECHILD) {
            /* Child already exited */
            session->state = MD_SESSION_STATE_TERMINATED;
            if (exit_code != NULL) {
                *exit_code = session->exit_code;
            }
            return MD_SUCCESS;
        }
        return MD_ERROR_UNKNOWN;
    }
    
    /* Process exited */
    session->state = MD_SESSION_STATE_TERMINATED;
    
    if (WIFEXITED(status)) {
        session->exit_code = WEXITSTATUS(status);
        session->signaled = false;
    } else if (WIFSIGNALED(status)) {
        session->exit_code = 128 + WTERMSIG(status);
        session->signaled = true;
        session->term_signal = WTERMSIG(status);
    }
    
    if (exit_code != NULL) {
        *exit_code = session->exit_code;
    }
    
    MD_DEBUG("Session %d (pid=%d) exited with code %d",
             session->id, session->pid, session->exit_code);
    
    return MD_SUCCESS;
}

bool md_session_is_running(md_session_t *session) {
    if (session == NULL || session->pid <= 0) {
        return false;
    }
    
    return kill(session->pid, 0) == 0;
}

md_session_state_t md_session_get_state(md_session_t *session) {
    if (session == NULL) {
        return MD_SESSION_STATE_NONE;
    }
    return session->state;
}

/* ============================================================================
 * Communication
 * ============================================================================ */

md_error_t md_session_send(md_session_t *session, const void *data, size_t size, size_t *bytes_sent) {
    if (session == NULL || data == NULL || size == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    if (session->stdin_fd < 0) {
        return MD_ERROR_INVALID_STATE;
    }
    
    if (session->state != MD_SESSION_STATE_RUNNING) {
        return MD_ERROR_INVALID_STATE;
    }
    
    ssize_t result = md_nb_write_all(session->stdin_fd, data, size);
    
    if (result < 0) {
        return (md_error_t)result;
    }
    
    session->bytes_sent += result;
    session->messages_sent++;
    
    if (bytes_sent != NULL) {
        *bytes_sent = (size_t)result;
    }
    
    MD_TRACE("Sent %zd bytes to session %d", result, session->id);
    
    return MD_SUCCESS;
}

md_error_t md_session_send_string(md_session_t *session, const char *str, size_t *bytes_sent) {
    if (str == NULL) {
        return MD_ERROR_NULL_POINTER;
    }
    return md_session_send(session, str, strlen(str), bytes_sent);
}

md_error_t md_session_receive(md_session_t *session, void *buffer, size_t size, size_t *bytes_received) {
    if (session == NULL || buffer == NULL || size == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    if (session->stdout_fd < 0) {
        return MD_ERROR_INVALID_STATE;
    }
    
    ssize_t result = md_nb_read(session->stdout_fd, buffer, size);
    
    if (result < 0) {
        return (md_error_t)result;
    }
    
    if (result > 0) {
        session->bytes_received += result;
        session->messages_received++;
    }
    
    if (bytes_received != NULL) {
        *bytes_received = (size_t)result;
    }
    
    return MD_SUCCESS;
}

md_error_t md_session_receive_line(md_session_t *session, char *buffer, size_t size, size_t *line_len) {
    if (session == NULL || buffer == NULL || size == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    if (session->stdout_fd < 0) {
        return MD_ERROR_INVALID_STATE;
    }
    
    /* Try to read a line from buffer first */
    size_t buf_line_len;
    if (md_io_buffer_has_line(session->read_buffer, &buf_line_len)) {
        if (buf_line_len >= size) {
            return MD_ERROR_BUFFER_OVERFLOW;
        }
        
        ssize_t n = md_io_buffer_read_line(session->read_buffer, buffer, size);
        if (n > 0) {
            session->bytes_received += n;
            session->messages_received++;
            
            if (line_len != NULL) {
                *line_len = (size_t)n;
            }
            return MD_SUCCESS;
        }
    }
    
    /* Need to read more data */
    char read_buf[4096];
    ssize_t n = md_nb_read(session->stdout_fd, read_buf, sizeof(read_buf));
    
    if (n < 0) {
        return (md_error_t)n;
    }
    
    if (n == 0) {
        /* No data available */
        if (line_len != NULL) {
            *line_len = 0;
        }
        buffer[0] = '\0';
        return MD_SUCCESS;
    }
    
    /* Add to buffer */
    md_io_buffer_write(session->read_buffer, read_buf, n);
    
    /* Try again */
    if (md_io_buffer_has_line(session->read_buffer, &buf_line_len)) {
        if (buf_line_len >= size) {
            return MD_ERROR_BUFFER_OVERFLOW;
        }
        
        n = md_io_buffer_read_line(session->read_buffer, buffer, size);
        if (n > 0) {
            session->bytes_received += n;
            session->messages_received++;
            
            if (line_len != NULL) {
                *line_len = (size_t)n;
            }
            return MD_SUCCESS;
        }
    }
    
    /* Still no complete line */
    if (line_len != NULL) {
        *line_len = 0;
    }
    buffer[0] = '\0';
    
    return MD_SUCCESS;
}

md_error_t md_session_read_stderr(md_session_t *session, void *buffer, size_t size, size_t *bytes_read) {
    if (session == NULL || buffer == NULL || size == 0) {
        return MD_ERROR_INVALID_ARG;
    }
    
    if (session->stderr_fd < 0) {
        return MD_ERROR_INVALID_STATE;
    }
    
    ssize_t result = md_nb_read(session->stderr_fd, buffer, size);
    
    if (result < 0) {
        return (md_error_t)result;
    }
    
    if (bytes_read != NULL) {
        *bytes_read = (size_t)result;
    }
    
    return MD_SUCCESS;
}

/* ============================================================================
 * File Descriptor Access
 * ============================================================================ */

int md_session_get_stdin_fd(md_session_t *session) {
    if (session == NULL) {
        return -1;
    }
    return session->stdin_fd;
}

int md_session_get_stdout_fd(md_session_t *session) {
    if (session == NULL) {
        return -1;
    }
    return session->stdout_fd;
}

int md_session_get_stderr_fd(md_session_t *session) {
    if (session == NULL) {
        return -1;
    }
    return session->stderr_fd;
}

int md_session_get_pid(md_session_t *session) {
    if (session == NULL) {
        return -1;
    }
    return session->pid;
}
