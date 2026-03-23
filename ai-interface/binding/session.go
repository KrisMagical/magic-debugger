package binding

/*
#cgo CFLAGS: -I${SRCDIR}/../../include -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
#cgo LDFLAGS: -L${SRCDIR}/../../build -lmagic_debugger -lpthread

#include <types.h>
#include <logger.h>
#include <session_manager.h>
#include <stdlib.h>
#include <string.h>
*/
import "C"
import (
    "runtime"
    "sync"
    "unsafe"
)

// LogLevel represents logging level
type LogLevel int

const (
    LogLevelTrace LogLevel = iota
    LogLevelDebug
    LogLevelInfo
    LogLevelWarning
    LogLevelError
    LogLevelFatal
    LogLevelNone
)

func cLogLevel(level LogLevel) C.md_log_level_t {
    return C.md_log_level_t(level)
}

// ManagerConfig represents session manager configuration
type ManagerConfig struct {
    MaxSessions int
    IOTimeoutMs int
    AutoCleanup bool
}

// DefaultManagerConfig returns default manager configuration
func DefaultManagerConfig() ManagerConfig {
    return ManagerConfig{
        MaxSessions: 16,
        IOTimeoutMs: 5000,
        AutoCleanup: true,
    }
}

// SessionManager manages debug sessions
type SessionManager struct {
    manager *C.md_session_manager_t
    mu      sync.RWMutex
}

// NewSessionManager creates a new session manager
func NewSessionManager(config *ManagerConfig) (*SessionManager, error) {
    m := C.md_manager_create(nil)
    if m == nil {
        return nil, ErrOutOfMemory
    }
    manager := &SessionManager{manager: m}
    runtime.SetFinalizer(manager, func(_ interface{}) {
        C.md_manager_destroy(m)
    })
    return manager, nil
}

// Destroy destroys the session manager
func (m *SessionManager) Destroy() {
    m.mu.Lock()
    defer m.mu.Unlock()
    if m.manager != nil {
        C.md_manager_destroy(m.manager)
        m.manager = nil
    }
}

// GetSessionCount returns the number of active sessions
func (m *SessionManager) GetSessionCount() int {
    m.mu.RLock()
    defer m.mu.RUnlock()
    if m.manager == nil {
        return -1
    }
    return int(C.md_manager_get_session_count(m.manager))
}

// SessionConfig represents session configuration
type SessionConfig struct {
    DebuggerPath string
    ProgramPath  string
    WorkingDir   string
    Args         []string
    DebuggerType DebuggerType
}

// Session represents a debug session
type Session struct {
    session *C.md_session_t
    manager *SessionManager
    mu      sync.RWMutex
}

// NewSession creates a new debug session
func (m *SessionManager) NewSession(config *SessionConfig) (*Session, error) {
    m.mu.Lock()
    defer m.mu.Unlock()
    if m.manager == nil {
        return nil, ErrNotInitialized
    }
    var cc C.md_session_config_t
    C.md_session_config_init(&cc)
    if config.DebuggerPath != "" {
        cDebuggerPath := C.CString(config.DebuggerPath)
        C.md_session_config_set_debugger(&cc, cDebuggerPath)
        C.free(unsafe.Pointer(cDebuggerPath))
    }
    if config.ProgramPath != "" {
        cProgramPath := C.CString(config.ProgramPath)
        C.md_session_config_set_program(&cc, cProgramPath)
        C.free(unsafe.Pointer(cProgramPath))
    }
    s := C.md_session_create(m.manager, &cc)
    if s == nil {
        return nil, ErrUnknown
    }
    session := &Session{session: s, manager: m}
    runtime.SetFinalizer(session, func(_ interface{}) {
        C.md_session_destroy(s)
    })
    return session, nil
}

// Destroy destroys the session
func (s *Session) Destroy() error {
    s.mu.Lock()
    defer s.mu.Unlock()
    if s.session == nil {
        return ErrNotInitialized
    }
    C.md_session_destroy(s.session)
    s.session = nil
    return nil
}

// GetID returns the session ID
func (s *Session) GetID() int {
    s.mu.RLock()
    defer s.mu.RUnlock()
    if s.session == nil {
        return -1
    }
    return int(C.md_session_get_id(s.session))
}

// GetPID returns the process ID
func (s *Session) GetPID() int {
    s.mu.RLock()
    defer s.mu.RUnlock()
    if s.session == nil {
        return -1
    }
    return int(C.md_session_get_pid(s.session))
}

// IsRunning checks if the session is running
func (s *Session) IsRunning() bool {
    s.mu.RLock()
    defer s.mu.RUnlock()
    if s.session == nil {
        return false
    }
    return bool(C.md_session_is_running(s.session))
}

// GetState returns the session state
func (s *Session) GetState() ExecState {
    s.mu.RLock()
    defer s.mu.RUnlock()
    if s.session == nil {
        return ExecStateNone
    }
    return ExecState(C.md_session_get_state(s.session))
}

// Terminate terminates the debugger process
func (s *Session) Terminate(force bool) error {
    s.mu.Lock()
    defer s.mu.Unlock()
    if s.session == nil {
        return ErrNotInitialized
    }
    return cError(C.md_session_terminate(s.session, C.bool(force)))
}

// SendString sends a string to the debugger stdin
func (s *Session) SendString(str string) (int, error) {
    s.mu.Lock()
    defer s.mu.Unlock()
    if s.session == nil {
        return 0, ErrNotInitialized
    }
    var bytesSent C.size_t
    cstr := C.CString(str)
    defer C.free(unsafe.Pointer(cstr))
    err := cError(C.md_session_send_string(s.session, cstr, &bytesSent))
    return int(bytesSent), err
}

// Receive receives data from the debugger stdout
func (s *Session) Receive(buf []byte) (int, error) {
    s.mu.Lock()
    defer s.mu.Unlock()
    if s.session == nil {
        return 0, ErrNotInitialized
    }
    var bytesReceived C.size_t
    err := cError(C.md_session_receive(s.session, unsafe.Pointer(&buf[0]), C.size_t(len(buf)), &bytesReceived))
    return int(bytesReceived), err
}

// GetStdinFD returns stdin file descriptor
func (s *Session) GetStdinFD() int {
    s.mu.RLock()
    defer s.mu.RUnlock()
    if s.session == nil {
        return -1
    }
    return int(C.md_session_get_stdin_fd(s.session))
}

// GetStdoutFD returns stdout file descriptor
func (s *Session) GetStdoutFD() int {
    s.mu.RLock()
    defer s.mu.RUnlock()
    if s.session == nil {
        return -1
    }
    return int(C.md_session_get_stdout_fd(s.session))
}
