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
	"sync"
	"unsafe"
)

// Logger represents a debug logger
type Logger struct {
	logger *C.md_logger_t
}

// LogLevel represents logging level
type LogLevel int

const (
	LogLevelNone LogLevel = iota
	LogLevelError
	LogLevelWarn
	LogLevelInfo
	LogLevelDebug
	LogLevelTrace
)

func goLogLevel(level C.md_log_level_t) LogLevel {
	return LogLevel(level)
}

func cLogLevel(level LogLevel) C.md_log_level_t {
	return C.md_log_level_t(level)
}

// NewLogger creates a new logger
func NewLogger(level LogLevel) *Logger {
	l := C.md_logger_create(cLogLevel(level))
	if l == nil {
		return nil
	}
	logger := &Logger{logger: l}
	setFinalizer(logger, func() {
		C.md_logger_destroy(l)
	})
	return logger
}

// ManagerConfig represents session manager configuration
type ManagerConfig struct {
	MaxSessions  int
	IOTimeoutMs  int
	AutoCleanup  bool
}

// cManagerConfig converts Go config to C
func (c *ManagerConfig) toC() C.md_manager_config_t {
	var cc C.md_manager_config_t
	C.md_manager_config_init(&cc)
	cc.max_sessions = C.int(c.MaxSessions)
	cc.io_timeout_ms = C.int(c.IOTimeoutMs)
	cc.auto_cleanup = C.bool(c.AutoCleanup)
	return cc
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
	var cc *C.md_manager_config_t
	if config != nil {
		c := config.toC()
		cc = &c
	}
	
	m := C.md_manager_create(cc)
	if m == nil {
		return nil, ErrOutOfMemory
	}
	
	manager := &SessionManager{manager: m}
	setFinalizer(manager, func() {
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

// SessionExists checks if a session exists
func (m *SessionManager) SessionExists(sessionID int) bool {
	m.mu.RLock()
	defer m.mu.RUnlock()
	if m.manager == nil {
		return false
	}
	return bool(C.md_manager_session_exists(m.manager, C.int(sessionID)))
}

// SessionConfig represents session configuration
type SessionConfig struct {
	DebuggerPath string
	ProgramPath  string
	WorkingDir   string
	Args         []string
	DebuggerType DebuggerType
	RedirectStderr bool
	CreatePTY      bool
}

// cSessionConfig converts Go config to C
func (c *SessionConfig) toC() C.md_session_config_t {
	var cc C.md_session_config_t
	C.md_session_config_init(&cc)
	
	if c.DebuggerPath != "" {
		cDebuggerPath := cString(c.DebuggerPath)
		C.md_session_config_set_debugger(&cc, cDebuggerPath)
		freeCString(cDebuggerPath)
	}
	
	if c.ProgramPath != "" {
		cProgramPath := cString(c.ProgramPath)
		C.md_session_config_set_program(&cc, cProgramPath)
		freeCString(cProgramPath)
	}
	
	if c.WorkingDir != "" {
		cstr := cString(c.WorkingDir)
		C.strcpy(&cc.working_dir[0], cstr)
		freeCString(cstr)
	}
	
	for _, arg := range c.Args {
		carg := cString(arg)
		C.md_session_config_add_arg(&cc, carg)
		freeCString(carg)
	}
	
	cc.debugger_type = goDebuggerType(c.DebuggerType)
	cc.redirect_stderr = C.bool(c.RedirectStderr)
	cc.create_pty = C.bool(c.CreatePTY)
	
	return cc
}

// SessionInfo represents session information
type SessionInfo struct {
	SessionID    int
	PID          int
	StdinFD      int
	StdoutFD     int
	StderrFD     int
	State        ExecState
	DebuggerType DebuggerType
	DebuggerName string
	ProgramPath  string
	ExitCode     int
	IsValid      bool
}

// cSessionInfo converts C session info to Go
func cSessionInfo(info *C.md_session_info_t) SessionInfo {
	return SessionInfo{
		SessionID:    int(info.session_id),
		PID:          int(info.pid),
		StdinFD:      int(info.stdin_fd),
		StdoutFD:     int(info.stdout_fd),
		StderrFD:     int(info.stderr_fd),
		State:        ExecState(info.state),
		DebuggerType: cDebuggerType(info.debugger_type),
		DebuggerName: goString(&info.debugger_name[0]),
		ProgramPath:  goString(&info.program_path[0]),
		ExitCode:     int(info.exit_code),
		IsValid:      bool(info.is_valid),
	}
}

// SessionStats represents session statistics
type SessionStats struct {
	BytesSent        uint64
	BytesReceived    uint64
	MessagesSent     uint64
	MessagesReceived uint64
	UptimeMs         uint64
	RestartCount     int
}

// cSessionStats converts C session stats to Go
func cSessionStats(stats *C.md_session_stats_t) SessionStats {
	return SessionStats{
		BytesSent:        uint64(stats.bytes_sent),
		BytesReceived:    uint64(stats.bytes_received),
		MessagesSent:     uint64(stats.messages_sent),
		MessagesReceived: uint64(stats.messages_received),
		UptimeMs:         uint64(stats.uptime_ms),
		RestartCount:     int(stats.restart_count),
	}
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
	
	cc := config.toC()
	s := C.md_session_create(m.manager, &cc)
	if s == nil {
		return nil, ErrUnknown
	}
	
	session := &Session{session: s, manager: m}
	setFinalizer(session, func() {
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
	
	err := cError(C.md_session_destroy(s.session))
	s.session = nil
	return err
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

// GetInfo returns session information
func (s *Session) GetInfo() (SessionInfo, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	
	var info C.md_session_info_t
	err := cError(C.md_session_get_info(s.session, &info))
	if err != nil {
		return SessionInfo{}, err
	}
	return cSessionInfo(&info), nil
}

// GetStats returns session statistics
func (s *Session) GetStats() (SessionStats, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	
	var stats C.md_session_stats_t
	err := cError(C.md_session_get_stats(s.session, &stats))
	if err != nil {
		return SessionStats{}, err
	}
	return cSessionStats(&stats), nil
}

// GetDebuggerType returns the debugger type
func (s *Session) GetDebuggerType() DebuggerType {
	s.mu.RLock()
	defer s.mu.RUnlock()
	
	if s.session == nil {
		return DebuggerNone
	}
	return cDebuggerType(C.md_session_get_debugger_type(s.session))
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

// Wait waits for the debugger process to exit
func (s *Session) Wait(timeoutMs int) (int, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	
	if s.session == nil {
		return -1, ErrNotInitialized
	}
	
	var exitCode C.int
	err := cError(C.md_session_wait(s.session, C.int(timeoutMs), &exitCode))
	return int(exitCode), err
}

// IsRunning checks if the debugger process is running
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

// Send sends data to the debugger's stdin
func (s *Session) Send(data []byte) (int, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	
	if s.session == nil {
		return 0, ErrNotInitialized
	}
	
	var bytesSent C.size_t
	err := cError(C.md_session_send(s.session, 
		unsafe.Pointer(&data[0]), 
		C.size_t(len(data)), 
		&bytesSent))
	return int(bytesSent), err
}

// SendString sends a string to the debugger's stdin
func (s *Session) SendString(str string) (int, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	
	if s.session == nil {
		return 0, ErrNotInitialized
	}
	
	var bytesSent C.size_t
	cstr := cString(str)
	defer freeCString(cstr)
	
	err := cError(C.md_session_send_string(s.session, cstr, &bytesSent))
	return int(bytesSent), err
}

// Receive receives data from the debugger's stdout
func (s *Session) Receive(buf []byte) (int, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	
	if s.session == nil {
		return 0, ErrNotInitialized
	}
	
	var bytesReceived C.size_t
	err := cError(C.md_session_receive(s.session,
		unsafe.Pointer(&buf[0]),
		C.size_t(len(buf)),
		&bytesReceived))
	return int(bytesReceived), err
}

// ReceiveLine receives a line from the debugger's stdout
func (s *Session) ReceiveLine(buf []byte) (int, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	
	if s.session == nil {
		return 0, ErrNotInitialized
	}
	
	var lineLen C.size_t
	err := cError(C.md_session_receive_line(s.session,
		(*C.char)(unsafe.Pointer(&buf[0])),
		C.size_t(len(buf)),
		&lineLen))
	return int(lineLen), err
}

// GetStdinFD returns the stdin file descriptor
func (s *Session) GetStdinFD() int {
	s.mu.RLock()
	defer s.mu.RUnlock()
	
	if s.session == nil {
		return -1
	}
	return int(C.md_session_get_stdin_fd(s.session))
}

// GetStdoutFD returns the stdout file descriptor
func (s *Session) GetStdoutFD() int {
	s.mu.RLock()
	defer s.mu.RUnlock()
	
	if s.session == nil {
		return -1
	}
	return int(C.md_session_get_stdout_fd(s.session))
}

// GetStderrFD returns the stderr file descriptor
func (s *Session) GetStderrFD() int {
	s.mu.RLock()
	defer s.mu.RUnlock()
	
	if s.session == nil {
		return -1
	}
	return int(C.md_session_get_stderr_fd(s.session))
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

// FindDebugger finds a debugger by type
func FindDebugger(debuggerType DebuggerType) (string, error) {
	var path [C.MD_MAX_PATH_LEN]C.char
	err := cError(C.md_find_debugger(goDebuggerType(debuggerType), &path[0], C.MD_MAX_PATH_LEN))
	if err != nil {
		return "", err
	}
	return goString(&path[0]), nil
}

// DetectDebuggerType detects debugger type from path
func DetectDebuggerType(path string) DebuggerType {
	cpath := cString(path)
	defer freeCString(cpath)
	return cDebuggerType(C.md_detect_debugger_type(cpath))
}

// DebuggerExists checks if a debugger exists at the given path
func DebuggerExists(path string) bool {
	cpath := cString(path)
	defer freeCString(cpath)
	return bool(C.md_debugger_exists(cpath))
}
