package binding

/*
#cgo CFLAGS: -I${SRCDIR}/../../include -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
#cgo LDFLAGS: -L${SRCDIR}/../../build -lmagic_debugger -lpthread

#include <types.h>
#include <adapter.h>
#include <state_model.h>
#include <stdlib.h>
#include <string.h>
*/
import "C"
import (
	"sync"
	"unsafe"
)

// AdapterState represents adapter state
type AdapterState int

const (
	AdapterStateNone       AdapterState = iota
	AdapterStateInitialized
	AdapterStateLaunching
	AdapterStateRunning
	AdapterStateStopped
	AdapterStateTerminated
	AdapterStateError
)

func (s AdapterState) String() string {
	switch s {
	case AdapterStateNone:
		return "none"
	case AdapterStateInitialized:
		return "initialized"
	case AdapterStateLaunching:
		return "launching"
	case AdapterStateRunning:
		return "running"
	case AdapterStateStopped:
		return "stopped"
	case AdapterStateTerminated:
		return "terminated"
	case AdapterStateError:
		return "error"
	default:
		return "unknown"
	}
}

func cAdapterState(s C.md_adapter_state_t) AdapterState {
	return AdapterState(s)
}

// AdapterConfig represents adapter configuration
type AdapterConfig struct {
	DebuggerPath      string
	DebuggerArgs      string
	ProgramPath       string
	ProgramArgs       string
	WorkingDir        string
	Environment       string
	StopOnEntry       bool
	AutoDisassemble   bool
	SourceContextLines int
	TimeoutMs         int
	Verbosity         int
}

// toC converts Go config to C
func (c *AdapterConfig) toC() C.md_adapter_config_t {
	var cc C.md_adapter_config_t
	C.md_adapter_config_init(&cc)
	
	if c.DebuggerPath != "" {
		C.md_adapter_config_set_debugger(&cc, cString(c.DebuggerPath))
	}
	if c.ProgramPath != "" {
		C.md_adapter_config_set_program(&cc, cString(c.ProgramPath))
	}
	if c.ProgramArgs != "" {
		C.md_adapter_config_set_args(&cc, cString(c.ProgramArgs))
	}
	if c.WorkingDir != "" {
		C.md_adapter_config_set_cwd(&cc, cString(c.WorkingDir))
	}
	
	cc.stop_on_entry = C.bool(c.StopOnEntry)
	cc.auto_disassemble = C.bool(c.AutoDisassemble)
	cc.source_context_lines = C.int(c.SourceContextLines)
	cc.timeout_ms = C.int(c.TimeoutMs)
	cc.verbosity = C.int(c.Verbosity)
	
	return cc
}

// DefaultAdapterConfig returns default adapter configuration
func DefaultAdapterConfig() AdapterConfig {
	return AdapterConfig{
		StopOnEntry:        false,
		AutoDisassemble:    true,
		SourceContextLines: 5,
		TimeoutMs:          5000,
		Verbosity:          0,
	}
}

// SourceLine represents a source line for display
type SourceLine struct {
	LineNumber       int
	Content          string
	IsExecutable     bool
	HasBreakpoint    bool
	IsCurrent        bool
	BreakpointID     int
	BreakpointMarker string
}

// cSourceLine converts C source line to Go
func cSourceLine(l *C.md_source_line_t) SourceLine {
	return SourceLine{
		LineNumber:       int(l.line_number),
		Content:          goString(&l.content[0]),
		IsExecutable:     bool(l.is_executable),
		HasBreakpoint:    bool(l.has_breakpoint),
		IsCurrent:        bool(l.is_current),
		BreakpointID:     int(l.breakpoint_id),
		BreakpointMarker: goString(&l.breakpoint_marker[0]),
	}
}

// SourceContext represents source file display context
type SourceContext struct {
	Path        string
	CurrentLine int
	StartLine   int
	EndLine     int
	TotalLines  int
	Lines       []SourceLine
}

// cSourceContext converts C source context to Go
func cSourceContext(ctx *C.md_source_context_t) *SourceContext {
	lines := make([]SourceLine, int(ctx.line_count))
	for i := 0; i < int(ctx.line_count); i++ {
		lines[i] = cSourceLine((*C.md_source_line_t)(unsafe.Pointer(
			uintptr(unsafe.Pointer(ctx.lines)) + uintptr(i)*unsafe.Sizeof(C.md_source_line_t{}),
		)))
	}
	return &SourceContext{
		Path:        goString(&ctx.path[0]),
		CurrentLine: int(ctx.current_line),
		StartLine:   int(ctx.start_line),
		EndLine:     int(ctx.end_line),
		TotalLines:  int(ctx.total_lines),
		Lines:       lines,
	}
}

// EvalResult represents variable evaluation result
type EvalResult struct {
	Success          bool
	Value            string
	Type             string
	Error            string
	VariablesRef     int
	NamedChildren    int
	IndexedChildren  int
}

// cEvalResult converts C eval result to Go
func cEvalResult(r *C.md_eval_result_t) EvalResult {
	return EvalResult{
		Success:         bool(r.success),
		Value:           goString(&r.value[0]),
		Type:            goString(&r._type[0]),
		Error:           goString(&r.error[0]),
		VariablesRef:    int(r.variables_reference),
		NamedChildren:   int(r.named_children),
		IndexedChildren: int(r.indexed_children),
	}
}

// Adapter represents a debugger adapter
type Adapter struct {
	adapter *C.md_adapter_t
	mu      sync.RWMutex
}

// NewAdapter creates a new debugger adapter
func NewAdapter(debuggerType DebuggerType, config *AdapterConfig) (*Adapter, error) {
	var cc C.md_adapter_config_t
	if config != nil {
		cc = config.toC()
	} else {
		C.md_adapter_config_init(&cc)
	}
	
	a := C.md_adapter_create(goDebuggerType(debuggerType), &cc)
	if a == nil {
		return nil, ErrOutOfMemory
	}
	
	adapter := &Adapter{adapter: a}
	setFinalizer(adapter, func() {
		C.md_adapter_destroy(a)
	})
	return adapter, nil
}

// NewAdapterByName creates a new debugger adapter by name
func NewAdapterByName(name string, config *AdapterConfig) (*Adapter, error) {
	var cc C.md_adapter_config_t
	if config != nil {
		cc = config.toC()
	} else {
		C.md_adapter_config_init(&cc)
	}
	
	cname := cString(name)
	defer freeCString(cname)
	
	a := C.md_adapter_create_by_name(cname, &cc)
	if a == nil {
		return nil, ErrNotFound
	}
	
	adapter := &Adapter{adapter: a}
	setFinalizer(adapter, func() {
		C.md_adapter_destroy(a)
	})
	return adapter, nil
}

// Destroy destroys the adapter
func (a *Adapter) Destroy() {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter != nil {
		C.md_adapter_destroy(a.adapter)
		a.adapter = nil
	}
}

// Init initializes the adapter
func (a *Adapter) Init() error {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return ErrNotInitialized
	}
	return cError(C.md_adapter_init(a.adapter))
}

// Launch launches the debuggee program
func (a *Adapter) Launch() error {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return ErrNotInitialized
	}
	return cError(C.md_adapter_launch(a.adapter))
}

// Attach attaches to a running process
func (a *Adapter) Attach(pid int) error {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return ErrNotInitialized
	}
	return cError(C.md_adapter_attach(a.adapter, C.int(pid)))
}

// Disconnect disconnects from the debugger
func (a *Adapter) Disconnect(terminateDebuggee bool) error {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return ErrNotInitialized
	}
	return cError(C.md_adapter_disconnect(a.adapter, C.bool(terminateDebuggee)))
}

// GetState returns the adapter state
func (a *Adapter) GetState() AdapterState {
	a.mu.RLock()
	defer a.mu.RUnlock()
	
	if a.adapter == nil {
		return AdapterStateNone
	}
	return cAdapterState(C.md_adapter_get_state(a.adapter))
}

// GetName returns the adapter name
func (a *Adapter) GetName() string {
	a.mu.RLock()
	defer a.mu.RUnlock()
	
	if a.adapter == nil {
		return ""
	}
	return goString(C.md_adapter_get_name(a.adapter))
}

// GetVersion returns the adapter version
func (a *Adapter) GetVersion() string {
	a.mu.RLock()
	defer a.mu.RUnlock()
	
	if a.adapter == nil {
		return ""
	}
	return goString(C.md_adapter_get_version(a.adapter))
}

// SupportsFeature checks if a feature is supported
func (a *Adapter) SupportsFeature(feature string) bool {
	a.mu.RLock()
	defer a.mu.RUnlock()
	
	if a.adapter == nil {
		return false
	}
	cfeature := cString(feature)
	defer freeCString(cfeature)
	return bool(C.md_adapter_supports_feature(a.adapter, cfeature))
}

// === Breakpoint Operations ===

// SetBreakpoint sets a line breakpoint
func (a *Adapter) SetBreakpoint(path string, line int, condition string) (int, error) {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return -1, ErrNotInitialized
	}
	
	var bpID C.int
	cpath := cString(path)
	defer freeCString(cpath)
	
	var ccondition *C.char
	if condition != "" {
		ccondition = cString(condition)
		defer freeCString(ccondition)
	}
	
	err := cError(C.md_adapter_set_breakpoint(a.adapter, cpath, C.int(line), ccondition, &bpID))
	return int(bpID), err
}

// SetFunctionBreakpoint sets a function breakpoint
func (a *Adapter) SetFunctionBreakpoint(function string) (int, error) {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return -1, ErrNotInitialized
	}
	
	var bpID C.int
	cfunction := cString(function)
	defer freeCString(cfunction)
	
	err := cError(C.md_adapter_set_function_breakpoint(a.adapter, cfunction, &bpID))
	return int(bpID), err
}

// RemoveBreakpoint removes a breakpoint
func (a *Adapter) RemoveBreakpoint(bpID int) error {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return ErrNotInitialized
	}
	return cError(C.md_adapter_remove_breakpoint(a.adapter, C.int(bpID)))
}

// EnableBreakpoint enables or disables a breakpoint
func (a *Adapter) EnableBreakpoint(bpID int, enable bool) error {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return ErrNotInitialized
	}
	return cError(C.md_adapter_enable_breakpoint(a.adapter, C.int(bpID), C.bool(enable)))
}

// SetBreakpointCondition sets a breakpoint condition
func (a *Adapter) SetBreakpointCondition(bpID int, condition string) error {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return ErrNotInitialized
	}
	ccondition := cString(condition)
	defer freeCString(ccondition)
	return cError(C.md_adapter_set_breakpoint_condition(a.adapter, C.int(bpID), ccondition))
}

// ClearAllBreakpoints clears all breakpoints
func (a *Adapter) ClearAllBreakpoints() error {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return ErrNotInitialized
	}
	return cError(C.md_adapter_clear_all_breakpoints(a.adapter))
}

// === Execution Control ===

// Continue continues program execution
func (a *Adapter) Continue() error {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return ErrNotInitialized
	}
	return cError(C.md_adapter_continue(a.adapter))
}

// Pause pauses program execution
func (a *Adapter) Pause() error {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return ErrNotInitialized
	}
	return cError(C.md_adapter_pause(a.adapter))
}

// StepOver steps over the next line
func (a *Adapter) StepOver(granularity StepGranularity) error {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return ErrNotInitialized
	}
	return cError(C.md_adapter_step_over(a.adapter, goStepGranularity(granularity)))
}

// StepInto steps into the next statement
func (a *Adapter) StepInto(granularity StepGranularity) error {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return ErrNotInitialized
	}
	return cError(C.md_adapter_step_into(a.adapter, goStepGranularity(granularity)))
}

// StepOut steps out of the current function
func (a *Adapter) StepOut() error {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return ErrNotInitialized
	}
	return cError(C.md_adapter_step_out(a.adapter))
}

// Restart restarts the debuggee
func (a *Adapter) Restart() error {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return ErrNotInitialized
	}
	return cError(C.md_adapter_restart(a.adapter))
}

// RunToLocation runs to a specific location
func (a *Adapter) RunToLocation(path string, line int) error {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return ErrNotInitialized
	}
	cpath := cString(path)
	defer freeCString(cpath)
	return cError(C.md_adapter_run_to_location(a.adapter, cpath, C.int(line)))
}

// === Variable Operations ===

// Evaluate evaluates an expression
func (a *Adapter) Evaluate(expression string, frameID int) (EvalResult, error) {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return EvalResult{}, ErrNotInitialized
	}
	
	var result C.md_eval_result_t
	cexpr := cString(expression)
	defer freeCString(cexpr)
	
	err := cError(C.md_adapter_evaluate(a.adapter, cexpr, C.int(frameID), &result))
	if err != nil {
		return EvalResult{}, err
	}
	return cEvalResult(&result), nil
}

// SetVariable sets a variable value
func (a *Adapter) SetVariable(varRef int, name, value string) error {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return ErrNotInitialized
	}
	cname := cString(name)
	defer freeCString(cname)
	cvalue := cString(value)
	defer freeCString(cvalue)
	
	return cError(C.md_adapter_set_variable(a.adapter, C.int(varRef), cname, cvalue))
}

// GetVariables gets variables in a scope
func (a *Adapter) GetVariables(varRef int, maxCount int) ([]Variable, error) {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return nil, ErrNotInitialized
	}
	
	variables := allocCVariableArray(maxCount)
	defer freeCArray(unsafe.Pointer(variables))
	
	var actualCount C.int
	err := cError(C.md_adapter_get_variables(a.adapter, C.int(varRef), variables, C.int(maxCount), &actualCount))
	if err != nil {
		return nil, err
	}
	
	result := make([]Variable, int(actualCount))
	for i := 0; i < int(actualCount); i++ {
		result[i] = *cVariable((*C.md_variable_t)(unsafe.Pointer(
			uintptr(unsafe.Pointer(variables)) + uintptr(i)*unsafe.Sizeof(C.md_variable_t{}),
		)))
	}
	return result, nil
}

// GetScopes gets scopes for a frame
func (a *Adapter) GetScopes(frameID int, maxCount int) ([]Scope, error) {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return nil, ErrNotInitialized
	}
	
	scopes := allocCScopeArray(maxCount)
	defer freeCArray(unsafe.Pointer(scopes))
	
	var actualCount C.int
	err := cError(C.md_adapter_get_scopes(a.adapter, C.int(frameID), scopes, C.int(maxCount), &actualCount))
	if err != nil {
		return nil, err
	}
	
	result := make([]Scope, int(actualCount))
	for i := 0; i < int(actualCount); i++ {
		result[i] = *cScope((*C.md_scope_t)(unsafe.Pointer(
			uintptr(unsafe.Pointer(scopes)) + uintptr(i)*unsafe.Sizeof(C.md_scope_t{}),
		)))
	}
	return result, nil
}

// PrintVariable prints a variable value
func (a *Adapter) PrintVariable(varName string) (string, error) {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return "", ErrNotInitialized
	}
	
	var output [4096]C.char
	cname := cString(varName)
	defer freeCString(cname)
	
	err := cError(C.md_adapter_print_variable(a.adapter, cname, &output[0], 4096))
	if err != nil {
		return "", err
	}
	return goString(&output[0]), nil
}

// === Stack and Thread Operations ===

// GetThreads gets all threads
func (a *Adapter) GetThreads(maxCount int) ([]Thread, error) {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return nil, ErrNotInitialized
	}
	
	threads := allocCThreadArray(maxCount)
	defer freeCArray(unsafe.Pointer(threads))
	
	var actualCount C.int
	err := cError(C.md_adapter_get_threads(a.adapter, threads, C.int(maxCount), &actualCount))
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

// GetStackFrames gets stack frames for a thread
func (a *Adapter) GetStackFrames(threadID int, maxCount int) ([]StackFrame, error) {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return nil, ErrNotInitialized
	}
	
	frames := allocCStackFrameArray(maxCount)
	defer freeCArray(unsafe.Pointer(frames))
	
	var actualCount C.int
	err := cError(C.md_adapter_get_stack_frames(a.adapter, C.int(threadID), frames, C.int(maxCount), &actualCount))
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

// SetActiveThread sets the active thread
func (a *Adapter) SetActiveThread(threadID int) error {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return ErrNotInitialized
	}
	return cError(C.md_adapter_set_active_thread(a.adapter, C.int(threadID)))
}

// SetActiveFrame sets the active stack frame
func (a *Adapter) SetActiveFrame(frameID int) error {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return ErrNotInitialized
	}
	return cError(C.md_adapter_set_active_frame(a.adapter, C.int(frameID)))
}

// GetCurrentThreadID gets the current thread ID
func (a *Adapter) GetCurrentThreadID() int {
	a.mu.RLock()
	defer a.mu.RUnlock()
	
	if a.adapter == nil {
		return -1
	}
	return int(C.md_adapter_get_current_thread_id(a.adapter))
}

// GetCurrentFrameID gets the current frame ID
func (a *Adapter) GetCurrentFrameID() int {
	a.mu.RLock()
	defer a.mu.RUnlock()
	
	if a.adapter == nil {
		return -1
	}
	return int(C.md_adapter_get_current_frame_id(a.adapter))
}

// === Source Code Display ===

// GetSource gets source file content
func (a *Adapter) GetSource(path string, startLine, endLine int) (*SourceContext, error) {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return nil, ErrNotInitialized
	}
	
	var ctx C.md_source_context_t
	cpath := cString(path)
	defer freeCString(cpath)
	
	err := cError(C.md_adapter_get_source(a.adapter, cpath, C.int(startLine), C.int(endLine), &ctx))
	if err != nil {
		return nil, err
	}
	defer C.md_adapter_free_source_context(&ctx)
	
	return cSourceContext(&ctx), nil
}

// Disassemble disassembles at an address
func (a *Adapter) Disassemble(address uint64) (string, error) {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return "", ErrNotInitialized
	}
	
	var output [4096]C.char
	err := cError(C.md_adapter_disassemble(a.adapter, C.uint64_t(address), &output[0], 4096))
	if err != nil {
		return "", err
	}
	return goString(&output[0]), nil
}

// === State Query Operations ===

// GetException gets exception information
func (a *Adapter) GetException() (Exception, error) {
	a.mu.RLock()
	defer a.mu.RUnlock()
	
	if a.adapter == nil {
		return Exception{}, ErrNotInitialized
	}
	
	var exc C.md_exception_t
	err := cError(C.md_adapter_get_exception(a.adapter, &exc))
	if err != nil {
		return Exception{}, err
	}
	return *cException(&exc), nil
}

// GetDebugState gets complete debug state
func (a *Adapter) GetDebugState() (DebugState, error) {
	a.mu.RLock()
	defer a.mu.RUnlock()
	
	if a.adapter == nil {
		return DebugState{}, ErrNotInitialized
	}
	
	var state C.md_debug_state_t
	err := cError(C.md_adapter_get_debug_state(a.adapter, &state))
	if err != nil {
		return DebugState{}, err
	}
	return *cDebugState(&state), nil
}

// ProcessEvents processes pending events
func (a *Adapter) ProcessEvents(timeoutMs int) error {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return ErrNotInitialized
	}
	return cError(C.md_adapter_process_events(a.adapter, C.int(timeoutMs)))
}

// WaitForStop waits for stop event
func (a *Adapter) WaitForStop(timeoutMs int) (StopReason, error) {
	a.mu.Lock()
	defer a.mu.Unlock()
	
	if a.adapter == nil {
		return StopReasonUnknown, ErrNotInitialized
	}
	
	var reason C.md_stop_reason_t
	err := cError(C.md_adapter_wait_for_stop(a.adapter, C.int(timeoutMs), &reason))
	if err != nil {
		return StopReasonUnknown, err
	}
	return cStopReason(reason), nil
}

// FindDebugger finds a debugger executable
func FindDebugger(debuggerType DebuggerType) (string, error) {
	var path [C.MD_MAX_PATH_LEN]C.char
	err := cError(C.md_adapter_find_debugger(goDebuggerType(debuggerType), &path[0], C.MD_MAX_PATH_LEN))
	if err != nil {
		return "", err
	}
	return goString(&path[0]), nil
}

// DetectDebuggerType detects debugger type from path
func DetectDebuggerType(path string) DebuggerType {
	cpath := cString(path)
	defer freeCString(cpath)
	return cDebuggerType(C.md_adapter_detect_type(cpath))
}
