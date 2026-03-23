package binding

/*
#cgo CFLAGS: -I${SRCDIR}/../../include -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
#cgo LDFLAGS: -L${SRCDIR}/../../build -lmagic_debugger -lpthread

#include <types.h>
#include <state_model.h>
#include <stdlib.h>
#include <string.h>
*/
import "C"
import (
	"sync"
	"unsafe"
)

// StateModel represents the unified debug state model
type StateModel struct {
	model *C.md_state_model_t
	mu    sync.RWMutex
}

// StateConfig represents state model configuration
type StateConfig struct {
	MaxBreakpoints     int
	MaxThreads         int
	MaxStackFrames     int
	MaxVariables       int
	MaxModules         int
	AutoUpdateThreads  bool
	AutoUpdateStack    bool
	AutoUpdateVariables bool
}

// toC converts Go config to C
func (c *StateConfig) toC() C.md_state_config_t {
	var cc C.md_state_config_t
	C.md_state_config_init(&cc)
	
	if c.MaxBreakpoints > 0 {
		cc.max_breakpoints = C.int(c.MaxBreakpoints)
	}
	if c.MaxThreads > 0 {
		cc.max_threads = C.int(c.MaxThreads)
	}
	if c.MaxStackFrames > 0 {
		cc.max_stack_frames = C.int(c.MaxStackFrames)
	}
	if c.MaxVariables > 0 {
		cc.max_variables = C.int(c.MaxVariables)
	}
	if c.MaxModules > 0 {
		cc.max_modules = C.int(c.MaxModules)
	}
	cc.auto_update_threads = C.bool(c.AutoUpdateThreads)
	cc.auto_update_stack = C.bool(c.AutoUpdateStack)
	cc.auto_update_variables = C.bool(c.AutoUpdateVariables)
	
	return cc
}

// DefaultStateConfig returns default state model configuration
func DefaultStateConfig() StateConfig {
	return StateConfig{
		MaxBreakpoints:     1024,
		MaxThreads:         128,
		MaxStackFrames:     256,
		MaxVariables:       1024,
		MaxModules:         256,
		AutoUpdateThreads:  true,
		AutoUpdateStack:    true,
		AutoUpdateVariables: true,
	}
}

// NewStateModel creates a new state model
func NewStateModel(config *StateConfig) (*StateModel, error) {
	var cc *C.md_state_config_t
	if config != nil {
		c := config.toC()
		cc = &c
	}
	
	m := C.md_state_model_create(cc)
	if m == nil {
		return nil, ErrOutOfMemory
	}
	
	model := &StateModel{model: m}
	setFinalizer(model, func() {
		C.md_state_model_destroy(m)
	})
	return model, nil
}

// Destroy destroys the state model
func (m *StateModel) Destroy() {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if m.model != nil {
		C.md_state_model_destroy(m.model)
		m.model = nil
	}
}

// Reset resets the state model to initial state
func (m *StateModel) Reset() error {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if m.model == nil {
		return ErrNotInitialized
	}
	return cError(C.md_state_model_reset(m.model))
}

// === Execution State ===

// GetExecState gets the current execution state
func (m *StateModel) GetExecState() ExecState {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if m.model == nil {
		return ExecStateNone
	}
	return cExecState(C.md_state_get_exec_state(m.model))
}

// SetExecState sets the execution state
func (m *StateModel) SetExecState(state ExecState) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if m.model == nil {
		return ErrNotInitialized
	}
	return cError(C.md_state_set_exec_state(m.model, C.md_exec_state_t(state)))
}

// GetStopReason gets the stop reason
func (m *StateModel) GetStopReason() StopReason {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if m.model == nil {
		return StopReasonUnknown
	}
	return cStopReason(C.md_state_get_stop_reason(m.model))
}

// SetStopReason sets the stop reason
func (m *StateModel) SetStopReason(reason StopReason, description string) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if m.model == nil {
		return ErrNotInitialized
	}
	
	cdesc := cString(description)
	defer freeCString(cdesc)
	return cError(C.md_state_set_stop_reason(m.model, C.md_stop_reason_t(reason), cdesc))
}

// GetCurrentThreadID gets the current thread ID
func (m *StateModel) GetCurrentThreadID() int {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if m.model == nil {
		return -1
	}
	return int(C.md_state_get_current_thread_id(m.model))
}

// SetCurrentThreadID sets the current thread ID
func (m *StateModel) SetCurrentThreadID(threadID int) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if m.model == nil {
		return ErrNotInitialized
	}
	return cError(C.md_state_set_current_thread_id(m.model, C.int(threadID)))
}

// GetCurrentFrameID gets the current frame ID
func (m *StateModel) GetCurrentFrameID() int {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if m.model == nil {
		return -1
	}
	return int(C.md_state_get_current_frame_id(m.model))
}

// SetCurrentFrameID sets the current frame ID
func (m *StateModel) SetCurrentFrameID(frameID int) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if m.model == nil {
		return ErrNotInitialized
	}
	return cError(C.md_state_set_current_frame_id(m.model, C.int(frameID)))
}

// GetDebugState gets complete debug state snapshot
func (m *StateModel) GetDebugState() (DebugState, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if m.model == nil {
		return DebugState{}, ErrNotInitialized
	}
	
	var state C.md_debug_state_t
	err := cError(C.md_state_get_debug_state(m.model, &state))
	if err != nil {
		return DebugState{}, err
	}
	return *cDebugState(&state), nil
}

// === Breakpoint Management ===

// AddBreakpoint adds a breakpoint
func (m *StateModel) AddBreakpoint(bp *Breakpoint) (int, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if m.model == nil {
		return -1, ErrNotInitialized
	}
	
	var cbp C.md_breakpoint_t
	C.md_breakpoint_init(&cbp)
	
	if bp.Path != "" {
		C.strcpy(&cbp.path[0], cString(bp.Path))
	}
	cbp.line = C.int(bp.Line)
	cbp.column = C.int(bp.Column)
	if bp.Condition != "" {
		C.strcpy(&cbp.condition[0], cString(bp.Condition))
	}
	if bp.FunctionName != "" {
		C.strcpy(&cbp.function_name[0], cString(bp.FunctionName))
	}
	cbp._type = C.md_bp_type_t(bp.Type)
	
	id := C.md_state_add_breakpoint(m.model, &cbp)
	if id < 0 {
		return -1, cError(C.md_error_t(id))
	}
	bp.ID = int(id)
	return int(id), nil
}

// UpdateBreakpoint updates a breakpoint
func (m *StateModel) UpdateBreakpoint(bp *Breakpoint) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if m.model == nil {
		return ErrNotInitialized
	}
	
	var cbp C.md_breakpoint_t
	C.md_breakpoint_init(&cbp)
	cbp.id = C.int(bp.ID)
	cbp.dap_id = C.int(bp.DAPID)
	cbp._type = C.md_bp_type_t(bp.Type)
	cbp.state = C.md_bp_state_t(bp.State)
	if bp.Path != "" {
		C.strcpy(&cbp.path[0], cString(bp.Path))
	}
	cbp.line = C.int(bp.Line)
	cbp.enabled = C.bool(bp.Enabled)
	cbp.verified = C.bool(bp.Verified)
	
	return cError(C.md_state_update_breakpoint(m.model, &cbp))
}

// RemoveBreakpoint removes a breakpoint
func (m *StateModel) RemoveBreakpoint(id int) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if m.model == nil {
		return ErrNotInitialized
	}
	return cError(C.md_state_remove_breakpoint(m.model, C.int(id)))
}

// GetBreakpoint gets a breakpoint by ID
func (m *StateModel) GetBreakpoint(id int) (*Breakpoint, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if m.model == nil {
		return nil, ErrNotInitialized
	}
	
	var bp C.md_breakpoint_t
	err := cError(C.md_state_get_breakpoint(m.model, C.int(id), &bp))
	if err != nil {
		return nil, err
	}
	return cBreakpoint(&bp), nil
}

// FindBreakpointByDAPID finds a breakpoint by DAP ID
func (m *StateModel) FindBreakpointByDAPID(dapID int) (*Breakpoint, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if m.model == nil {
		return nil, ErrNotInitialized
	}
	
	var bp C.md_breakpoint_t
	err := cError(C.md_state_find_breakpoint_by_dap_id(m.model, C.int(dapID), &bp))
	if err != nil {
		return nil, err
	}
	return cBreakpoint(&bp), nil
}

// FindBreakpointByLocation finds a breakpoint by file and line
func (m *StateModel) FindBreakpointByLocation(path string, line int) (*Breakpoint, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if m.model == nil {
		return nil, ErrNotInitialized
	}
	
	var bp C.md_breakpoint_t
	cpath := cString(path)
	defer freeCString(cpath)
	
	err := cError(C.md_state_find_breakpoint_by_location(m.model, cpath, C.int(line), &bp))
	if err != nil {
		return nil, err
	}
	return cBreakpoint(&bp), nil
}

// GetAllBreakpoints gets all breakpoints
func (m *StateModel) GetAllBreakpoints(maxCount int) ([]Breakpoint, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if m.model == nil {
		return nil, ErrNotInitialized
	}
	
	breakpoints := allocCBreakpointArray(maxCount)
	defer freeCArray(unsafe.Pointer(breakpoints))
	
	var actualCount C.int
	err := cError(C.md_state_get_all_breakpoints(m.model, breakpoints, C.int(maxCount), &actualCount))
	if err != nil {
		return nil, err
	}
	
	result := make([]Breakpoint, int(actualCount))
	for i := 0; i < int(actualCount); i++ {
		result[i] = *cBreakpoint((*C.md_breakpoint_t)(unsafe.Pointer(
			uintptr(unsafe.Pointer(breakpoints)) + uintptr(i)*unsafe.Sizeof(C.md_breakpoint_t{}),
		)))
	}
	return result, nil
}

// GetBreakpointCount gets the number of breakpoints
func (m *StateModel) GetBreakpointCount() int {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if m.model == nil {
		return 0
	}
	return int(C.md_state_get_breakpoint_count(m.model))
}

// ClearAllBreakpoints clears all breakpoints
func (m *StateModel) ClearAllBreakpoints() error {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if m.model == nil {
		return ErrNotInitialized
	}
	return cError(C.md_state_clear_all_breakpoints(m.model))
}

// SetBreakpointEnabled enables or disables a breakpoint
func (m *StateModel) SetBreakpointEnabled(id int, enabled bool) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if m.model == nil {
		return ErrNotInitialized
	}
	return cError(C.md_state_set_breakpoint_enabled(m.model, C.int(id), C.bool(enabled)))
}

// === Thread Management ===

// AddThread adds a thread
func (m *StateModel) AddThread(thread *Thread) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if m.model == nil {
		return ErrNotInitialized
	}
	
	var ct C.md_thread_t
	C.md_thread_init(&ct)
	ct.id = C.int(thread.ID)
	if thread.Name != "" {
		C.strcpy(&ct.name[0], cString(thread.Name))
	}
	ct.state = C.md_thread_state_t(thread.State)
	ct.is_main = C.bool(thread.IsMain)
	
	return cError(C.md_state_add_thread(m.model, &ct))
}

// UpdateThread updates a thread
func (m *StateModel) UpdateThread(thread *Thread) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if m.model == nil {
		return ErrNotInitialized
	}
	
	var ct C.md_thread_t
	ct.id = C.int(thread.ID)
	if thread.Name != "" {
		C.strcpy(&ct.name[0], cString(thread.Name))
	}
	ct.state = C.md_thread_state_t(thread.State)
	ct.is_stopped = C.bool(thread.IsStopped)
	
	return cError(C.md_state_update_thread(m.model, &ct))
}

// RemoveThread removes a thread
func (m *StateModel) RemoveThread(threadID int) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if m.model == nil {
		return ErrNotInitialized
	}
	return cError(C.md_state_remove_thread(m.model, C.int(threadID)))
}

// GetThread gets a thread by ID
func (m *StateModel) GetThread(threadID int) (*Thread, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if m.model == nil {
		return nil, ErrNotInitialized
	}
	
	var t C.md_thread_t
	err := cError(C.md_state_get_thread(m.model, C.int(threadID), &t))
	if err != nil {
		return nil, err
	}
	return cThread(&t), nil
}

// GetAllThreads gets all threads
func (m *StateModel) GetAllThreads(maxCount int) ([]Thread, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if m.model == nil {
		return nil, ErrNotInitialized
	}
	
	threads := allocCThreadArray(maxCount)
	defer freeCArray(unsafe.Pointer(threads))
	
	var actualCount C.int
	err := cError(C.md_state_get_all_threads(m.model, threads, C.int(maxCount), &actualCount))
	if err != nil {
		return nil, err
	}
	
	result := make([]Thread, int(actualCount))
	for i := 0; i < int(actualCount); i++ {
		result[i] = *cThread((*C.md_thread_t)(unsafe.Pointer(
			uintptr(unsafe.Pointer(threads)) + uintptr(i)*unsafe.Sizeof(C.md_thread_t{}),
		)))
	}
	return result, nil
}

// GetThreadCount gets the number of threads
func (m *StateModel) GetThreadCount() int {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if m.model == nil {
		return 0
	}
	return int(C.md_state_get_thread_count(m.model))
}

// ClearAllThreads clears all threads
func (m *StateModel) ClearAllThreads() error {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if m.model == nil {
		return ErrNotInitialized
	}
	return cError(C.md_state_clear_all_threads(m.model))
}

// === Stack Frame Management ===

// SetStackFrames sets stack frames for a thread
func (m *StateModel) SetStackFrames(threadID int, frames []StackFrame) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if m.model == nil {
		return ErrNotInitialized
	}
	
	cframes := allocCStackFrameArray(len(frames))
	defer freeCArray(unsafe.Pointer(cframes))
	
	for i, f := range frames {
		cf := (*C.md_stack_frame_t)(unsafe.Pointer(
			uintptr(unsafe.Pointer(cframes)) + uintptr(i)*unsafe.Sizeof(C.md_stack_frame_t{}),
		))
		cf.id = C.int(f.ID)
		cf.line = C.int(f.Line)
		cf.column = C.int(f.Column)
		if f.Name != "" {
			C.strcpy(&cf.name[0], cString(f.Name))
		}
		if f.SourcePath != "" {
			C.strcpy(&cf.source_path[0], cString(f.SourcePath))
		}
		cf.instruction_pointer = C.uint64_t(f.InstructionPointer)
	}
	
	return cError(C.md_state_set_stack_frames(m.model, C.int(threadID), cframes, C.int(len(frames))))
}

// GetStackFrames gets stack frames for a thread
func (m *StateModel) GetStackFrames(threadID int, maxCount int) ([]StackFrame, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if m.model == nil {
		return nil, ErrNotInitialized
	}
	
	frames := allocCStackFrameArray(maxCount)
	defer freeCArray(unsafe.Pointer(frames))
	
	var actualCount C.int
	err := cError(C.md_state_get_stack_frames(m.model, C.int(threadID), frames, C.int(maxCount), &actualCount))
	if err != nil {
		return nil, err
	}
	
	result := make([]StackFrame, int(actualCount))
	for i := 0; i < int(actualCount); i++ {
		result[i] = *cStackFrame((*C.md_stack_frame_t)(unsafe.Pointer(
			uintptr(unsafe.Pointer(frames)) + uintptr(i)*unsafe.Sizeof(C.md_stack_frame_t{}),
		)))
	}
	return result, nil
}

// GetStackFrameCount gets the stack frame count for a thread
func (m *StateModel) GetStackFrameCount(threadID int) int {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if m.model == nil {
		return 0
	}
	return int(C.md_state_get_stack_frame_count(m.model, C.int(threadID)))
}

// ClearStackFrames clears stack frames for a thread
func (m *StateModel) ClearStackFrames(threadID int) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if m.model == nil {
		return ErrNotInitialized
	}
	return cError(C.md_state_clear_stack_frames(m.model, C.int(threadID)))
}

// === Exception Management ===

// SetException sets exception information
func (m *StateModel) SetException(exc *Exception) error {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if m.model == nil {
		return ErrNotInitialized
	}
	
	var cexc C.md_exception_t
	C.md_exception_init(&cexc)
	if exc.Type != "" {
		C.strcpy(&cexc._type[0], cString(exc.Type))
	}
	if exc.Message != "" {
		C.strcpy(&cexc.message[0], cString(exc.Message))
	}
	if exc.Description != "" {
		C.strcpy(&cexc.description[0], cString(exc.Description))
	}
	if exc.StackTrace != "" {
		C.strcpy(&cexc.stack_trace[0], cString(exc.StackTrace))
	}
	cexc.thread_id = C.int(exc.ThreadID)
	cexc.line = C.int(exc.Line)
	cexc.is_uncaught = C.bool(exc.IsUncaught)
	
	return cError(C.md_state_set_exception(m.model, &cexc))
}

// GetException gets exception information
func (m *StateModel) GetException() (Exception, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if m.model == nil {
		return Exception{}, ErrNotInitialized
	}
	
	var exc C.md_exception_t
	err := cError(C.md_state_get_exception(m.model, &exc))
	if err != nil {
		return Exception{}, err
	}
	return *cException(&exc), nil
}

// ClearException clears exception information
func (m *StateModel) ClearException() error {
	m.mu.Lock()
	defer m.mu.Unlock()
	
	if m.model == nil {
		return ErrNotInitialized
	}
	return cError(C.md_state_clear_exception(m.model))
}

// HasException checks if there's an active exception
func (m *StateModel) HasException() bool {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if m.model == nil {
		return false
	}
	return bool(C.md_state_has_exception(m.model))
}

// Dump dumps state to string (for debugging)
func (m *StateModel) Dump() (string, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()
	
	if m.model == nil {
		return "", ErrNotInitialized
	}
	
	var buffer [8192]C.char
	err := cError(C.md_state_dump(m.model, &buffer[0], 8192))
	if err != nil {
		return "", err
	}
	return goString(&buffer[0]), nil
}
