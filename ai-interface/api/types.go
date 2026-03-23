// Package api provides debugger adapter types and constants
package api

// ============================================================================
// Adapter Types
// ============================================================================

// AdapterState represents the state of a debugger adapter
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

// StepGranularity represents step granularity
type StepGranularity int

const (
	StepGranularityLine StepGranularity = iota
	StepGranularityStatement
	StepGranularityInstruction
)

func (g StepGranularity) String() string {
	switch g {
	case StepGranularityLine:
		return "line"
	case StepGranularityStatement:
		return "statement"
	case StepGranularityInstruction:
		return "instruction"
	default:
		return "unknown"
	}
}

// SourceLine represents a source code line with debug info
type SourceLine struct {
	LineNumber       int    `json:"line_number"`
	Content          string `json:"content"`
	IsExecutable     bool   `json:"is_executable"`
	HasBreakpoint    bool   `json:"has_breakpoint"`
	IsCurrent        bool   `json:"is_current"`
	BreakpointID     int    `json:"breakpoint_id,omitempty"`
	BreakpointMarker string `json:"breakpoint_marker,omitempty"`
}

// SourceContext represents source file display context
type SourceContext struct {
	Path        string       `json:"path"`
	CurrentLine int          `json:"current_line"`
	StartLine   int          `json:"start_line"`
	EndLine     int          `json:"end_line"`
	TotalLines  int          `json:"total_lines"`
	Lines       []SourceLine `json:"lines"`
}

// EvalResult represents an expression evaluation result
type EvalResult struct {
	Success         bool   `json:"success"`
	Value           string `json:"value"`
	Type            string `json:"type"`
	Error           string `json:"error,omitempty"`
	VariablesRef    int    `json:"variables_reference"`
	NamedChildren   int    `json:"named_children"`
	IndexedChildren int    `json:"indexed_children"`
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
	ID              int       `json:"id"`
	Type            ScopeType `json:"type"`
	Name            string    `json:"name"`
	FrameID         int       `json:"frame_id"`
	NamedVariables  int       `json:"named_variables"`
	IndexedVariables int      `json:"indexed_variables"`
	Expensive       bool      `json:"expensive"`
}

// VariableKind represents variable presentation kind
type VariableKind int

const (
	VariableKindNormal VariableKind = iota
	VariableKindPointer
	VariableKindArray
	VariableKindObject
	VariableKindCollection
	VariableKindString
	VariableKindNull
	VariableKindFunction
	VariableKindVirtual
)

func (k VariableKind) String() string {
	switch k {
	case VariableKindNormal:
		return "normal"
	case VariableKindPointer:
		return "pointer"
	case VariableKindArray:
		return "array"
	case VariableKindObject:
		return "object"
	case VariableKindCollection:
		return "collection"
	case VariableKindString:
		return "string"
	case VariableKindNull:
		return "null"
	case VariableKindFunction:
		return "function"
	case VariableKindVirtual:
		return "virtual"
	default:
		return "unknown"
	}
}

// ModuleState represents module load state
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
	ID            int          `json:"id"`
	Name          string       `json:"name"`
	Path          string       `json:"path"`
	Version       string       `json:"version"`
	State         ModuleState  `json:"state"`
	SymbolsLoaded bool         `json:"symbols_loaded"`
	SymbolFile    string       `json:"symbol_file"`
	SymbolStatus  string       `json:"symbol_status"`
	BaseAddress   uint64       `json:"base_address"`
	Size          uint64       `json:"size"`
	IsOptimized   bool         `json:"is_optimized"`
	IsUserCode    bool         `json:"is_user_code"`
	IsSystem      bool         `json:"is_system"`
}

// Capabilities represents debugger capabilities
type Capabilities struct {
	SupportsConfigurationDoneRequest    bool     `json:"supports_configuration_done_request"`
	SupportsFunctionBreakpoints         bool     `json:"supports_function_breakpoints"`
	SupportsConditionalBreakpoints      bool     `json:"supports_conditional_breakpoints"`
	SupportsHitConditionalBreakpoints   bool     `json:"supports_hit_conditional_breakpoints"`
	SupportsEvaluateForHovers           bool     `json:"supports_evaluate_for_hovers"`
	SupportsStepBack                    bool     `json:"supports_step_back"`
	SupportsSetVariable                 bool     `json:"supports_set_variable"`
	SupportsRestartFrame                bool     `json:"supports_restart_frame"`
	SupportsGotoTargetsRequest          bool     `json:"supports_goto_targets_request"`
	SupportsStepInTargetsRequest        bool     `json:"supports_step_in_targets_request"`
	SupportsCompletionsRequest          bool     `json:"supports_completions_request"`
	SupportsModulesRequest              bool     `json:"supports_modules_request"`
	SupportsExceptionOptions            bool     `json:"supports_exception_options"`
	SupportsValueFormattingOptions      bool     `json:"supports_value_formatting_options"`
	SupportsExceptionInfoRequest        bool     `json:"supports_exception_info_request"`
	SupportTerminateDebuggee            bool     `json:"support_terminate_debuggee"`
	SupportsDelayedStackTraceLoading    bool     `json:"supports_delayed_stack_trace_loading"`
	SupportsLoadedSourcesRequest        bool     `json:"supports_loaded_sources_request"`
	SupportsLogPoints                   bool     `json:"supports_log_points"`
	SupportsTerminateThreadsRequest     bool     `json:"supports_terminate_threads_request"`
	SupportsSetExpression               bool     `json:"supports_set_expression"`
	SupportsTerminateRequest            bool     `json:"supports_terminate_request"`
	SupportsDataBreakpoints             bool     `json:"supports_data_breakpoints"`
	SupportsReadMemoryRequest           bool     `json:"supports_read_memory_request"`
	SupportsDisassembleRequest          bool     `json:"supports_disassemble_request"`
	SupportsCancelRequest               bool     `json:"supports_cancel_request"`
	SupportsBreakpointLocationsRequest  bool     `json:"supports_breakpoint_locations_request"`
	SupportsClipboardContext            bool     `json:"supports_clipboard_context"`
	SupportsSteppingGranularity         bool     `json:"supports_stepping_granularity"`
	SupportsInstructionBreakpoints      bool     `json:"supports_instruction_breakpoints"`
	SupportsExceptionFilterOptions      bool     `json:"supports_exception_filter_options"`
	SupportedChecksumAlgorithms         []string `json:"supported_checksum_algorithms,omitempty"`
}
