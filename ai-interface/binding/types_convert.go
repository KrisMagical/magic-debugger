package binding

/*
#cgo CFLAGS: -I${SRCDIR}/../../include -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
#cgo LDFLAGS: -L${SRCDIR}/../../build -lmagic_debugger -lpthread

#include <types.h>
#include <state_types.h>
#include <stdlib.h>
#include <string.h>
*/
import "C"
import "unsafe"

// Breakpoint represents a debug breakpoint
type Breakpoint struct {
	ID           int
	DAPID        int
	Type         BreakpointType
	State        BreakpointState
	Path         string
	Line         int
	Column       int
	FunctionName string
	Condition    string
	HitCondition string
	LogMessage   string
	HitCount     int
	IgnoreCount  int
	Enabled      bool
	Verified     bool
	Message      string
}

// cBreakpoint converts C breakpoint to Go
func cBreakpoint(bp *C.md_breakpoint_t) *Breakpoint {
	return &Breakpoint{
		ID:           int(bp.id),
		DAPID:        int(bp.dap_id),
		Type:         BreakpointType(bp._type),
		State:        BreakpointState(bp.state),
		Path:         goString(&bp.path[0]),
		Line:         int(bp.line),
		Column:       int(bp.column),
		FunctionName: goString(&bp.function_name[0]),
		Condition:    goString(&bp.condition[0]),
		HitCondition: goString(&bp.hit_condition[0]),
		LogMessage:   goString(&bp.log_message[0]),
		HitCount:     int(bp.hit_count),
		IgnoreCount:  int(bp.ignore_count),
		Enabled:      bool(bp.enabled),
		Verified:     bool(bp.verified),
		Message:      goString(&bp.message[0]),
	}
}

// Thread represents a debug thread
type Thread struct {
	ID             int
	Name           string
	State          ThreadState
	CurrentFrameID int
	IsStopped      bool
	IsMain         bool
}

// cThread converts C thread to Go
func cThread(t *C.md_thread_t) *Thread {
	return &Thread{
		ID:             int(t.id),
		Name:           goString(&t.name[0]),
		State:          ThreadState(t.state),
		CurrentFrameID: int(t.current_frame_id),
		IsStopped:      bool(t.is_stopped),
		IsMain:         bool(t.is_main),
	}
}

// StackFrame represents a call stack frame
type StackFrame struct {
	ID                 int
	Name               string
	Module             string
	SourcePath         string
	Line               int
	Column             int
	EndLine            int
	EndColumn          int
	InstructionPointer uint64
	ReturnAddress      uint64
	PresentationHint   string
	IsUserCode         bool
	ThreadID           int
}

// cStackFrame converts C stack frame to Go
func cStackFrame(f *C.md_stack_frame_t) *StackFrame {
	return &StackFrame{
		ID:                 int(f.id),
		Name:               goString(&f.name[0]),
		Module:             goString(&f.module[0]),
		SourcePath:         goString(&f.source_path[0]),
		Line:               int(f.line),
		Column:             int(f.column),
		EndLine:            int(f.end_line),
		EndColumn:          int(f.end_column),
		InstructionPointer: uint64(f.instruction_pointer),
		ReturnAddress:      uint64(f.return_address),
		PresentationHint:   goString(&f.presentation_hint[0]),
		IsUserCode:         bool(f.is_user_code),
		ThreadID:           int(f.thread_id),
	}
}

// ScopeType represents variable scope type
type ScopeType int

const (
	ScopeTypeLocals ScopeType = iota
	ScopeTypeArguments
	ScopeTypeGlobals
	ScopeTypeRegisters
	ScopeTypeClosure
	ScopeTypeReturn
)

func (t ScopeType) String() string {
	switch t {
	case ScopeTypeLocals:
		return "locals"
	case ScopeTypeArguments:
		return "arguments"
	case ScopeTypeGlobals:
		return "globals"
	case ScopeTypeRegisters:
		return "registers"
	case ScopeTypeClosure:
		return "closure"
	case ScopeTypeReturn:
		return "return"
	default:
		return "unknown"
	}
}

// Scope represents a variable scope
type Scope struct {
	ID              int
	Type            ScopeType
	Name            string
	FrameID         int
	NamedVariables  int
	IndexedVariables int
	Expensive       bool
}

// cScope converts C scope to Go
func cScope(s *C.md_scope_t) *Scope {
	return &Scope{
		ID:              int(s.id),
		Type:            ScopeType(s._type),
		Name:            goString(&s.name[0]),
		FrameID:         int(s.frame_id),
		NamedVariables:  int(s.named_variables),
		IndexedVariables: int(s.indexed_variables),
		Expensive:       bool(s.expensive),
	}
}

// VariableKind represents variable kind
type VariableKind int

const (
	VarKindNormal VariableKind = iota
	VarKindPointer
	VarKindArray
	VarKindObject
	VarKindCollection
	VarKindString
	VarKindNull
	VarKindFunction
	VarKindVirtual
)

func (k VariableKind) String() string {
	switch k {
	case VarKindNormal:
		return "normal"
	case VarKindPointer:
		return "pointer"
	case VarKindArray:
		return "array"
	case VarKindObject:
		return "object"
	case VarKindCollection:
		return "collection"
	case VarKindString:
		return "string"
	case VarKindNull:
		return "null"
	case VarKindFunction:
		return "function"
	case VarKindVirtual:
		return "virtual"
	default:
		return "unknown"
	}
}

// Variable represents a debug variable
type Variable struct {
	ID                int
	Name              string
	Value             string
	Type              string
	VariablesRef      int
	NamedChildren     int
	IndexedChildren   int
	ParentID          int
	EvaluateName      string
	Kind              VariableKind
	PresentationHint  string
	MemoryReference   uint64
	ScopeID           int
}

// cVariable converts C variable to Go
func cVariable(v *C.md_variable_t) *Variable {
	return &Variable{
		ID:               int(v.id),
		Name:             goString(&v.name[0]),
		Value:            goString(&v.value[0]),
		Type:             goString(&v._type[0]),
		VariablesRef:     int(v.variables_reference),
		NamedChildren:    int(v.named_children),
		IndexedChildren:  int(v.indexed_children),
		ParentID:         int(v.parent_id),
		EvaluateName:     goString(&v.evaluate_name[0]),
		Kind:             VariableKind(v.kind),
		PresentationHint: goString(&v.presentation_hint[0]),
		MemoryReference:  uint64(v.memory_reference),
		ScopeID:          int(v.scope_id),
	}
}

// ModuleState represents module state
type ModuleState int

const (
	ModuleStateUnknown ModuleState = iota
	ModuleStateLoading
	ModuleStateLoaded
	ModuleStateFailed
	ModuleStateUnloaded
)

func (s ModuleState) String() string {
	switch s {
	case ModuleStateUnknown:
		return "unknown"
	case ModuleStateLoading:
		return "loading"
	case ModuleStateLoaded:
		return "loaded"
	case ModuleStateFailed:
		return "failed"
	case ModuleStateUnloaded:
		return "unloaded"
	default:
		return "unknown"
	}
}

// Module represents a loaded module/library
type Module struct {
	ID            int
	Name          string
	Path          string
	Version       string
	State         ModuleState
	SymbolsLoaded bool
	SymbolFile    string
	SymbolStatus  string
	BaseAddress   uint64
	Size          uint64
	IsOptimized   bool
	IsUserCode    bool
	IsSystem      bool
}

// cModule converts C module to Go
func cModule(m *C.md_module_t) *Module {
	return &Module{
		ID:            int(m.id),
		Name:          goString(&m.name[0]),
		Path:          goString(&m.path[0]),
		Version:       goString(&m.version[0]),
		State:         ModuleState(m.state),
		SymbolsLoaded: bool(m.symbols_loaded),
		SymbolFile:    goString(&m.symbol_file[0]),
		SymbolStatus:  goString(&m.symbol_status[0]),
		BaseAddress:   uint64(m.base_address),
		Size:          uint64(m.size),
		IsOptimized:   bool(m.is_optimized),
		IsUserCode:    bool(m.is_user_code),
		IsSystem:      bool(m.is_system),
	}
}

// Exception represents exception information
type Exception struct {
	Type        string
	Message     string
	Description string
	StackTrace  string
	ThreadID    int
	FrameID     int
	SourcePath  string
	Line        int
	IsUncaught  bool
}

// cException converts C exception to Go
func cException(e *C.md_exception_t) *Exception {
	return &Exception{
		Type:        goString(&e._type[0]),
		Message:     goString(&e.message[0]),
		Description: goString(&e.description[0]),
		StackTrace:  goString(&e.stack_trace[0]),
		ThreadID:    int(e.thread_id),
		FrameID:     int(e.frame_id),
		SourcePath:  goString(&e.source_path[0]),
		Line:        int(e.line),
		IsUncaught:  bool(e.is_uncaught),
	}
}

// DebugState represents a complete debug state snapshot
type DebugState struct {
	// Execution State
	ExecState       ExecState
	StopReason      StopReason
	StopDescription string

	// Current Context
	CurrentThreadID int
	CurrentFrameID  int

	// Program Info
	ProgramPath string
	WorkingDir  string
	PID         int
	ExitCode    int

	// Exception
	Exception   Exception
	HasException bool

	// Timestamps
	StateTimestamp   uint64
	LastStopTimestamp uint64
	LastRunTimestamp  uint64

	// Statistics
	TotalStops          uint64
	TotalBreakpointHits uint64
	TotalSteps          uint64
}

// cDebugState converts C debug state to Go
func cDebugState(s *C.md_debug_state_t) *DebugState {
	return &DebugState{
		ExecState:          cExecState(s.exec_state),
		StopReason:         cStopReason(s.stop_reason),
		StopDescription:    goString(&s.stop_description[0]),
		CurrentThreadID:    int(s.current_thread_id),
		CurrentFrameID:     int(s.current_frame_id),
		ProgramPath:        goString(&s.program_path[0]),
		WorkingDir:         goString(&s.working_dir[0]),
		PID:                int(s.pid),
		ExitCode:           int(s.exit_code),
		Exception:          *cException(&s.exception),
		HasException:       bool(s.has_exception),
		StateTimestamp:     uint64(s.state_timestamp),
		LastStopTimestamp:  uint64(s.last_stop_timestamp),
		LastRunTimestamp:   uint64(s.last_run_timestamp),
		TotalStops:         uint64(s.total_stops),
		TotalBreakpointHits: uint64(s.total_breakpoint_hits),
		TotalSteps:         uint64(s.total_steps),
	}
}

// Max constants
const (
	MaxBreakpoints  = C.MD_MAX_BREAKPOINTS
	MaxStackFrames  = C.MD_MAX_STACK_FRAMES
	MaxThreads      = C.MD_MAX_THREADS
	MaxScopes       = C.MD_MAX_SCOPES
	MaxVariables    = C.MD_MAX_VARIABLES
	MaxModules      = C.MD_MAX_MODULES
	InvalidID       = C.MD_INVALID_ID
	MaxPathLen      = C.MD_MAX_PATH_LEN
	MaxVariableLen  = C.MD_MAX_VARIABLE_STR_LEN
)

// InitBreakpoint initializes a new breakpoint
func InitBreakpoint() *Breakpoint {
	var bp C.md_breakpoint_t
	C.md_breakpoint_init(&bp)
	return cBreakpoint(&bp)
}

// InitThread initializes a new thread
func InitThread() *Thread {
	var t C.md_thread_t
	C.md_thread_init(&t)
	return cThread(&t)
}

// InitStackFrame initializes a new stack frame
func InitStackFrame() *StackFrame {
	var f C.md_stack_frame_t
	C.md_stack_frame_init(&f)
	return cStackFrame(&f)
}

// InitVariable initializes a new variable
func InitVariable() *Variable {
	var v C.md_variable_t
	C.md_variable_init(&v)
	return cVariable(&v)
}

// InitScope initializes a new scope
func InitScope() *Scope {
	var s C.md_scope_t
	C.md_scope_init(&s)
	return cScope(&s)
}

// InitModule initializes a new module
func InitModule() *Module {
	var m C.md_module_t
	C.md_module_init(&m)
	return cModule(&m)
}

// InitException initializes a new exception
func InitException() *Exception {
	var e C.md_exception_t
	C.md_exception_init(&e)
	return cException(&e)
}

// InitDebugState initializes a new debug state
func InitDebugState() *DebugState {
	var s C.md_debug_state_t
	C.md_debug_state_init(&s)
	return cDebugState(&s)
}

// Helper to allocate C array
func allocCBreakpointArray(count int) *C.md_breakpoint_t {
	return (*C.md_breakpoint_t)(C.calloc(C.size_t(count), C.size_t(unsafe.Sizeof(C.md_breakpoint_t{}))))
}

func allocCThreadArray(count int) *C.md_thread_t {
	return (*C.md_thread_t)(C.calloc(C.size_t(count), C.size_t(unsafe.Sizeof(C.md_thread_t{}))))
}

func allocCStackFrameArray(count int) *C.md_stack_frame_t {
	return (*C.md_stack_frame_t)(C.calloc(C.size_t(count), C.size_t(unsafe.Sizeof(C.md_stack_frame_t{}))))
}

func allocCScopeArray(count int) *C.md_scope_t {
	return (*C.md_scope_t)(C.calloc(C.size_t(count), C.size_t(unsafe.Sizeof(C.md_scope_t{}))))
}

func allocCVariableArray(count int) *C.md_variable_t {
	return (*C.md_variable_t)(C.calloc(C.size_t(count), C.size_t(unsafe.Sizeof(C.md_variable_t{}))))
}

// Free C array
func freeCArray(ptr unsafe.Pointer) {
	C.free(ptr)
}
