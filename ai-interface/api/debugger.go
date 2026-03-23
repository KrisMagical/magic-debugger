// Package api provides high-level API for AI systems to interact with the debugger.
// This package abstracts the low-level bindings and provides a clean, idiomatic Go API.
package api

import (
        "context"
        "encoding/json"
        "fmt"
        "sync"
        "time"

        "github.com/magic-debugger/ai-interface/binding"
)

// DebuggerType represents the type of debugger backend
type DebuggerType string

const (
        DebuggerTypeLLDB  DebuggerType = "lldb"
        DebuggerTypeGDB   DebuggerType = "gdb"
        DebuggerTypeShell DebuggerType = "shell"
)

// ExecutionState represents program execution state
type ExecutionState string

const (
        StateNone       ExecutionState = "none"
        StateLaunching  ExecutionState = "launching"
        StateRunning    ExecutionState = "running"
        StateStopped    ExecutionState = "stopped"
        StateStepping   ExecutionState = "stepping"
        StateExiting    ExecutionState = "exiting"
        StateTerminated ExecutionState = "terminated"
        StateCrashed    ExecutionState = "crashed"
)

// EventType represents debugger event types
type EventType string

const (
        EventStateChanged    EventType = "state_changed"
        EventBreakpointHit   EventType = "breakpoint_hit"
        EventStepComplete    EventType = "step_complete"
        EventException       EventType = "exception"
        EventProcessExited   EventType = "process_exited"
        EventOutput          EventType = "output"
        EventVariableChanged EventType = "variable_changed"
)

// Event represents a debugger event
type Event struct {
        Type      EventType   `json:"type"`
        Timestamp time.Time   `json:"timestamp"`
        Data      interface{} `json:"data,omitempty"`
}

// EventHandler is a callback for debugger events
type EventHandler func(Event)

// BreakpointInfo represents breakpoint information for AI
type BreakpointInfo struct {
        ID          int    `json:"id"`
        File        string `json:"file"`
        Line        int    `json:"line"`
        Condition   string `json:"condition,omitempty"`
        HitCount    int    `json:"hit_count"`
        Enabled     bool   `json:"enabled"`
        Verified    bool   `json:"verified"`
}

// ThreadInfo represents thread information for AI
type ThreadInfo struct {
        ID       int    `json:"id"`
        Name     string `json:"name"`
        State    string `json:"state"`
        IsMain   bool   `json:"is_main"`
        Frames   int    `json:"frame_count"`
}

// StackFrameInfo represents stack frame information for AI
type StackFrameInfo struct {
        ID          int    `json:"id"`
        Function    string `json:"function"`
        File        string `json:"file,omitempty"`
        Line        int    `json:"line"`
        Address     uint64 `json:"address"`
        IsUserCode  bool   `json:"is_user_code"`
}

// VariableInfo represents variable information for AI
type VariableInfo struct {
        Name         string `json:"name"`
        Value        string `json:"value"`
        Type         string `json:"type,omitempty"`
        HasChildren  bool   `json:"has_children"`
        ChildCount   int    `json:"child_count,omitempty"`
        MemoryAddr   uint64 `json:"memory_address,omitempty"`
}

// SourceLocation represents a source code location
type SourceLocation struct {
        File   string `json:"file"`
        Line   int    `json:"line"`
        Column int    `json:"column,omitempty"`
}

// DebugSessionState represents the complete debug state for AI
type DebugSessionState struct {
        // Execution state
        ExecutionState ExecutionState `json:"execution_state"`
        StopReason     string         `json:"stop_reason,omitempty"`
        StopDescription string        `json:"stop_description,omitempty"`

        // Current location
        CurrentLocation *SourceLocation `json:"current_location,omitempty"`

        // Program info
        ProgramPath string `json:"program_path"`
        ProcessID   int    `json:"process_id,omitempty"`
        ExitCode    int    `json:"exit_code,omitempty"`

        // Current context
        CurrentThreadID int `json:"current_thread_id"`
        CurrentFrameID  int `json:"current_frame_id"`

        // Breakpoints
        Breakpoints []BreakpointInfo `json:"breakpoints,omitempty"`

        // Threads
        Threads []ThreadInfo `json:"threads,omitempty"`

        // Stack trace (current thread)
        StackTrace []StackFrameInfo `json:"stack_trace,omitempty"`

        // Local variables (current frame)
        LocalVariables []VariableInfo `json:"local_variables,omitempty"`

        // Exception info
        Exception *ExceptionInfo `json:"exception,omitempty"`

        // Timestamps
        Timestamp time.Time `json:"timestamp"`
}

// ExceptionInfo represents exception information
type ExceptionInfo struct {
        Type        string `json:"type"`
        Message     string `json:"message"`
        Description string `json:"description,omitempty"`
        StackTrace  string `json:"stack_trace,omitempty"`
        File        string `json:"file,omitempty"`
        Line        int    `json:"line,omitempty"`
        IsUncaught  bool   `json:"is_uncaught"`
}

// Config represents debugger configuration
type Config struct {
        DebuggerType   DebuggerType `json:"debugger_type"`
        DebuggerPath   string       `json:"debugger_path,omitempty"`
        ProgramPath    string       `json:"program_path"`
        ProgramArgs    []string     `json:"program_args,omitempty"`
        WorkingDir     string       `json:"working_dir,omitempty"`
        StopOnEntry    bool         `json:"stop_on_entry"`
        Timeout        time.Duration `json:"timeout"`
}

// DefaultConfig returns default configuration
func DefaultConfig() Config {
        return Config{
                DebuggerType: DebuggerTypeLLDB,
                StopOnEntry:  false,
                Timeout:      30 * time.Second,
        }
}

// Debugger is the main debugger interface for AI
type Debugger struct {
        mu          sync.RWMutex
        adapter     *binding.Adapter
        stateModel  *binding.StateModel
        config      Config
        running     bool
        eventCh     chan Event
        handlers    []EventHandler
        ctx         context.Context
        cancel      context.CancelFunc
}

// NewDebugger creates a new debugger instance
func NewDebugger(config Config) (*Debugger, error) {
        // Convert config
        adapterConfig := binding.DefaultAdapterConfig()
        adapterConfig.ProgramPath = config.ProgramPath
        adapterConfig.WorkingDir = config.WorkingDir
        adapterConfig.StopOnEntry = config.StopOnEntry
        
        if config.DebuggerPath != "" {
                adapterConfig.DebuggerPath = config.DebuggerPath
        }
        
        if config.Timeout > 0 {
                adapterConfig.TimeoutMs = int(config.Timeout.Milliseconds())
        }
        
        // Convert debugger type
        var debuggerType binding.DebuggerType
        switch config.DebuggerType {
        case DebuggerTypeLLDB:
                debuggerType = binding.DebuggerLLDB
        case DebuggerTypeGDB:
                debuggerType = binding.DebuggerGDB
        case DebuggerTypeShell:
                debuggerType = binding.DebuggerShell
        default:
                debuggerType = binding.DebuggerLLDB
        }
        
        // Create adapter
        adapter, err := binding.NewAdapter(debuggerType, &adapterConfig)
        if err != nil {
                return nil, fmt.Errorf("failed to create adapter: %w", err)
        }
        
        // Create state model
        stateConfig := binding.DefaultStateConfig()
        stateModel, err := binding.NewStateModel(&stateConfig)
        if err != nil {
                adapter.Destroy()
                return nil, fmt.Errorf("failed to create state model: %w", err)
        }
        
        ctx, cancel := context.WithCancel(context.Background())
        
        d := &Debugger{
                adapter:    adapter,
                stateModel: stateModel,
                config:     config,
                eventCh:    make(chan Event, 100),
                handlers:   make([]EventHandler, 0),
                ctx:        ctx,
                cancel:     cancel,
        }
        
        // Start event processor
        go d.processEvents()
        
        return d, nil
}

// Close closes the debugger
func (d *Debugger) Close() error {
        d.mu.Lock()
        defer d.mu.Unlock()
        
        if d.running {
                d.adapter.Disconnect(true)
                d.running = false
        }
        
        d.cancel()
        close(d.eventCh)
        
        d.adapter.Destroy()
        d.stateModel.Destroy()
        
        return nil
}

// Initialize initializes the debugger
func (d *Debugger) Initialize(ctx context.Context) error {
        d.mu.Lock()
        defer d.mu.Unlock()
        
        if err := d.adapter.Init(); err != nil {
                return fmt.Errorf("failed to initialize adapter: %w", err)
        }
        
        return nil
}

// Launch launches the debuggee program
func (d *Debugger) Launch(ctx context.Context) error {
        d.mu.Lock()
        defer d.mu.Unlock()
        
        if err := d.adapter.Launch(); err != nil {
                return fmt.Errorf("failed to launch program: %w", err)
        }
        
        d.running = true
        
        // Wait for initial stop (if stop on entry)
        if d.config.StopOnEntry {
                if _, err := d.adapter.WaitForStop(int(d.config.Timeout.Milliseconds())); err != nil {
                        return fmt.Errorf("timeout waiting for initial stop: %w", err)
                }
        }
        
        d.emitEvent(Event{
                Type:      EventStateChanged,
                Timestamp: time.Now(),
                Data:      d.GetState(),
        })
        
        return nil
}

// Attach attaches to a running process
func (d *Debugger) Attach(ctx context.Context, pid int) error {
        d.mu.Lock()
        defer d.mu.Unlock()
        
        if err := d.adapter.Attach(pid); err != nil {
                return fmt.Errorf("failed to attach to process %d: %w", pid, err)
        }
        
        d.running = true
        d.emitEvent(Event{
                Type:      EventStateChanged,
                Timestamp: time.Now(),
                Data:      d.GetState(),
        })
        
        return nil
}

// === State Query Operations ===

// GetState returns the current debug state
func (d *Debugger) GetState() DebugSessionState {
        d.mu.RLock()
        defer d.mu.RUnlock()
        
        state := DebugSessionState{
                Timestamp:      time.Now(),
                ProgramPath:    d.config.ProgramPath,
                CurrentThreadID: d.adapter.GetCurrentThreadID(),
                CurrentFrameID:  d.adapter.GetCurrentFrameID(),
        }
        
        // Get execution state
        adapterState := d.adapter.GetState()
        switch adapterState {
        case binding.AdapterStateNone:
                state.ExecutionState = StateNone
        case binding.AdapterStateInitialized:
                state.ExecutionState = StateLaunching
        case binding.AdapterStateLaunching:
                state.ExecutionState = StateLaunching
        case binding.AdapterStateRunning:
                state.ExecutionState = StateRunning
        case binding.AdapterStateStopped:
                state.ExecutionState = StateStopped
        case binding.AdapterStateTerminated:
                state.ExecutionState = StateTerminated
        case binding.AdapterStateError:
                state.ExecutionState = StateCrashed
        }
        
        // Get debug state from model
        if debugState, err := d.stateModel.GetDebugState(); err == nil {
                state.StopReason = debugState.StopReason.String()
                state.StopDescription = debugState.StopDescription
                state.ProcessID = debugState.PID
                state.ExitCode = debugState.ExitCode
                
                if debugState.HasException {
                        state.Exception = &ExceptionInfo{
                                Type:        debugState.Exception.Type,
                                Message:     debugState.Exception.Message,
                                Description: debugState.Exception.Description,
                                StackTrace:  debugState.Exception.StackTrace,
                                File:        debugState.Exception.SourcePath,
                                Line:        debugState.Exception.Line,
                                IsUncaught:  debugState.Exception.IsUncaught,
                        }
                }
                
                // Current location
                if debugState.CurrentThreadID > 0 {
                        if frames, err := d.stateModel.GetStackFrames(debugState.CurrentThreadID, 1); err == nil && len(frames) > 0 {
                                state.CurrentLocation = &SourceLocation{
                                        File:   frames[0].SourcePath,
                                        Line:   frames[0].Line,
                                        Column: frames[0].Column,
                                }
                        }
                }
        }
        
        // Get breakpoints
        if bps, err := d.stateModel.GetAllBreakpoints(100); err == nil {
                state.Breakpoints = make([]BreakpointInfo, len(bps))
                for i, bp := range bps {
                        state.Breakpoints[i] = BreakpointInfo{
                                ID:        bp.ID,
                                File:      bp.Path,
                                Line:      bp.Line,
                                Condition: bp.Condition,
                                HitCount:  bp.HitCount,
                                Enabled:   bp.Enabled,
                                Verified:  bp.Verified,
                        }
                }
        }
        
        // Get threads
        if threads, err := d.stateModel.GetAllThreads(100); err == nil {
                state.Threads = make([]ThreadInfo, len(threads))
                for i, t := range threads {
                        state.Threads[i] = ThreadInfo{
                                ID:     t.ID,
                                Name:   t.Name,
                                State:  t.State.String(),
                                IsMain: t.IsMain,
                        }
                }
        }
        
        // Get stack trace
        if state.CurrentThreadID > 0 {
                if frames, err := d.stateModel.GetStackFrames(state.CurrentThreadID, 50); err == nil {
                        state.StackTrace = make([]StackFrameInfo, len(frames))
                        for i, f := range frames {
                                state.StackTrace[i] = StackFrameInfo{
                                        ID:         f.ID,
                                        Function:   f.Name,
                                        File:       f.SourcePath,
                                        Line:       f.Line,
                                        Address:    f.InstructionPointer,
                                        IsUserCode: f.IsUserCode,
                                }
                        }
                }
        }
        
        return state
}

// GetStateJSON returns the current debug state as JSON
func (d *Debugger) GetStateJSON() (string, error) {
        state := d.GetState()
        data, err := json.MarshalIndent(state, "", "  ")
        if err != nil {
                return "", err
        }
        return string(data), nil
}

// GetBreakpoints returns all breakpoints
func (d *Debugger) GetBreakpoints() ([]BreakpointInfo, error) {
        d.mu.RLock()
        defer d.mu.RUnlock()
        
        bps, err := d.stateModel.GetAllBreakpoints(100)
        if err != nil {
                return nil, err
        }
        
        result := make([]BreakpointInfo, len(bps))
        for i, bp := range bps {
                result[i] = BreakpointInfo{
                        ID:        bp.ID,
                        File:      bp.Path,
                        Line:      bp.Line,
                        Condition: bp.Condition,
                        HitCount:  bp.HitCount,
                        Enabled:   bp.Enabled,
                        Verified:  bp.Verified,
                }
        }
        return result, nil
}

// GetThreads returns all threads
func (d *Debugger) GetThreads() ([]ThreadInfo, error) {
        d.mu.RLock()
        defer d.mu.RUnlock()
        
        threads, err := d.stateModel.GetAllThreads(100)
        if err != nil {
                return nil, err
        }
        
        result := make([]ThreadInfo, len(threads))
        for i, t := range threads {
                result[i] = ThreadInfo{
                        ID:     t.ID,
                        Name:   t.Name,
                        State:  t.State.String(),
                        IsMain: t.IsMain,
                }
        }
        return result, nil
}

// GetStackTrace returns the stack trace for a thread
func (d *Debugger) GetStackTrace(threadID int) ([]StackFrameInfo, error) {
        d.mu.RLock()
        defer d.mu.RUnlock()
        
        if threadID <= 0 {
                threadID = d.adapter.GetCurrentThreadID()
        }
        
        frames, err := d.stateModel.GetStackFrames(threadID, 100)
        if err != nil {
                return nil, err
        }
        
        result := make([]StackFrameInfo, len(frames))
        for i, f := range frames {
                result[i] = StackFrameInfo{
                        ID:         f.ID,
                        Function:   f.Name,
                        File:       f.SourcePath,
                        Line:       f.Line,
                        Address:    f.InstructionPointer,
                        IsUserCode: f.IsUserCode,
                }
        }
        return result, nil
}

// GetVariables returns variables for a scope
func (d *Debugger) GetVariables(frameID int, scopeType string) ([]VariableInfo, error) {
        d.mu.RLock()
        defer d.mu.RUnlock()
        
        if frameID <= 0 {
                frameID = d.adapter.GetCurrentFrameID()
        }
        
        // Get scopes for frame
        scopes, err := d.adapter.GetScopes(frameID, 10)
        if err != nil {
                return nil, err
        }
        
        // Find matching scope
        var targetScope *binding.Scope
        for _, s := range scopes {
                if scopeType == "locals" && s.Type == binding.ScopeTypeLocals {
                        targetScope = &s
                        break
                }
                if scopeType == "arguments" && s.Type == binding.ScopeTypeArguments {
                        targetScope = &s
                        break
                }
        }
        
        if targetScope == nil && len(scopes) > 0 {
                targetScope = &scopes[0]
        }
        
        if targetScope == nil {
                return []VariableInfo{}, nil
        }
        
        // Get variables
        vars, err := d.adapter.GetVariables(targetScope.ID, 100)
        if err != nil {
                return nil, err
        }
        
        result := make([]VariableInfo, len(vars))
        for i, v := range vars {
                result[i] = VariableInfo{
                        Name:        v.Name,
                        Value:       v.Value,
                        Type:        v.Type,
                        HasChildren: v.VariablesRef > 0,
                        ChildCount:  v.NamedChildren + v.IndexedChildren,
                        MemoryAddr:  v.MemoryReference,
                }
        }
        return result, nil
}

// Evaluate evaluates an expression
func (d *Debugger) Evaluate(expression string, frameID int) (VariableInfo, error) {
        d.mu.RLock()
        defer d.mu.RUnlock()
        
        if frameID <= 0 {
                frameID = d.adapter.GetCurrentFrameID()
        }
        
        result, err := d.adapter.Evaluate(expression, frameID)
        if err != nil {
                return VariableInfo{}, err
        }
        
        return VariableInfo{
                Name:        expression,
                Value:       result.Value,
                Type:        result.Type,
                HasChildren: result.VariablesRef > 0,
                ChildCount:  result.NamedChildren + result.IndexedChildren,
        }, nil
}

// === Execution Control Operations ===

// Continue continues program execution
func (d *Debugger) Continue(ctx context.Context) error {
        d.mu.Lock()
        defer d.mu.Unlock()
        
        if !d.running {
                return fmt.Errorf("debugger not running")
        }
        
        if err := d.adapter.Continue(); err != nil {
                return fmt.Errorf("continue failed: %w", err)
        }
        
        d.emitEvent(Event{
                Type:      EventStateChanged,
                Timestamp: time.Now(),
                Data:      "running",
        })
        
        return nil
}

// StepOver steps over the next line
func (d *Debugger) StepOver(ctx context.Context) error {
        d.mu.Lock()
        defer d.mu.Unlock()
        
        if !d.running {
                return fmt.Errorf("debugger not running")
        }
        
        if err := d.adapter.StepOver(binding.StepLine); err != nil {
                return fmt.Errorf("step over failed: %w", err)
        }
        
        d.emitEvent(Event{
                Type:      EventStateChanged,
                Timestamp: time.Now(),
                Data:      "stepping",
        })
        
        return nil
}

// StepInto steps into the next statement
func (d *Debugger) StepInto(ctx context.Context) error {
        d.mu.Lock()
        defer d.mu.Unlock()
        
        if !d.running {
                return fmt.Errorf("debugger not running")
        }
        
        if err := d.adapter.StepInto(binding.StepLine); err != nil {
                return fmt.Errorf("step into failed: %w", err)
        }
        
        d.emitEvent(Event{
                Type:      EventStateChanged,
                Timestamp: time.Now(),
                Data:      "stepping",
        })
        
        return nil
}

// StepOut steps out of the current function
func (d *Debugger) StepOut(ctx context.Context) error {
        d.mu.Lock()
        defer d.mu.Unlock()
        
        if !d.running {
                return fmt.Errorf("debugger not running")
        }
        
        if err := d.adapter.StepOut(); err != nil {
                return fmt.Errorf("step out failed: %w", err)
        }
        
        d.emitEvent(Event{
                Type:      EventStateChanged,
                Timestamp: time.Now(),
                Data:      "stepping",
        })
        
        return nil
}

// Pause pauses program execution
func (d *Debugger) Pause(ctx context.Context) error {
        d.mu.Lock()
        defer d.mu.Unlock()
        
        if !d.running {
                return fmt.Errorf("debugger not running")
        }
        
        if err := d.adapter.Pause(); err != nil {
                return fmt.Errorf("pause failed: %w", err)
        }
        
        return nil
}

// Restart restarts the debuggee
func (d *Debugger) Restart(ctx context.Context) error {
        d.mu.Lock()
        defer d.mu.Unlock()
        
        if !d.running {
                return fmt.Errorf("debugger not running")
        }
        
        if err := d.adapter.Restart(); err != nil {
                return fmt.Errorf("restart failed: %w", err)
        }
        
        d.emitEvent(Event{
                Type:      EventStateChanged,
                Timestamp: time.Now(),
                Data:      "restarted",
        })
        
        return nil
}

// === Breakpoint Operations ===

// SetBreakpoint sets a breakpoint at a file and line
func (d *Debugger) SetBreakpoint(file string, line int, condition string) (BreakpointInfo, error) {
        d.mu.Lock()
        defer d.mu.Unlock()
        
        bpID, err := d.adapter.SetBreakpoint(file, line, condition)
        if err != nil {
                return BreakpointInfo{}, fmt.Errorf("set breakpoint failed: %w", err)
        }
        
        // Add to state model
        bp := binding.InitBreakpoint()
        bp.ID = bpID
        bp.Path = file
        bp.Line = line
        bp.Condition = condition
        bp.Enabled = true
        
        if _, err := d.stateModel.AddBreakpoint(bp); err != nil {
                // Log but don't fail
        }
        
        return BreakpointInfo{
                ID:        bpID,
                File:      file,
                Line:      line,
                Condition: condition,
                Enabled:   true,
        }, nil
}

// RemoveBreakpoint removes a breakpoint
func (d *Debugger) RemoveBreakpoint(id int) error {
        d.mu.Lock()
        defer d.mu.Unlock()
        
        if err := d.adapter.RemoveBreakpoint(id); err != nil {
                return fmt.Errorf("remove breakpoint failed: %w", err)
        }
        
        d.stateModel.RemoveBreakpoint(id)
        return nil
}

// ToggleBreakpoint toggles a breakpoint
func (d *Debugger) ToggleBreakpoint(file string, line int) (BreakpointInfo, bool, error) {
        d.mu.Lock()
        defer d.mu.Unlock()
        
        // Check if breakpoint exists
        bp, err := d.stateModel.FindBreakpointByLocation(file, line)
        if err == nil && bp != nil {
                // Remove existing
                if err := d.adapter.RemoveBreakpoint(bp.ID); err != nil {
                        return BreakpointInfo{}, false, err
                }
                d.stateModel.RemoveBreakpoint(bp.ID)
                return BreakpointInfo{}, false, nil
        }
        
        // Create new
        bpID, err := d.adapter.SetBreakpoint(file, line, "")
        if err != nil {
                return BreakpointInfo{}, false, err
        }
        
        return BreakpointInfo{
                ID:      bpID,
                File:    file,
                Line:    line,
                Enabled: true,
        }, true, nil
}

// EnableBreakpoint enables a breakpoint
func (d *Debugger) EnableBreakpoint(id int) error {
        d.mu.Lock()
        defer d.mu.Unlock()
        return d.adapter.EnableBreakpoint(id, true)
}

// DisableBreakpoint disables a breakpoint
func (d *Debugger) DisableBreakpoint(id int) error {
        d.mu.Lock()
        defer d.mu.Unlock()
        return d.adapter.EnableBreakpoint(id, false)
}

// === Event Handling ===

// OnEvent registers an event handler
func (d *Debugger) OnEvent(handler EventHandler) {
        d.mu.Lock()
        defer d.mu.Unlock()
        d.handlers = append(d.handlers, handler)
}

// Events returns the event channel
func (d *Debugger) Events() <-chan Event {
        return d.eventCh
}

// emitEvent emits an event to all handlers
func (d *Debugger) emitEvent(event Event) {
        select {
        case d.eventCh <- event:
        default:
                // Channel full, drop event
        }
        
        for _, handler := range d.handlers {
                go handler(event)
        }
}

// processEvents processes events from the adapter
func (d *Debugger) processEvents() {
        ticker := time.NewTicker(100 * time.Millisecond)
        defer ticker.Stop()
        
        for {
                select {
                case <-d.ctx.Done():
                        return
                case <-ticker.C:
                        d.mu.Lock()
                        if d.running {
                                // Process pending events
                                d.adapter.ProcessEvents(0)
                                
                                // Check state changes
                                state := d.adapter.GetState()
                                if state == binding.AdapterStateStopped {
                                        d.emitEvent(Event{
                                                Type:      EventStateChanged,
                                                Timestamp: time.Now(),
                                                Data:      d.GetState(),
                                        })
                                }
                        }
                        d.mu.Unlock()
                }
        }
}

// WaitForStop waits for the program to stop
func (d *Debugger) WaitForStop(ctx context.Context, timeout time.Duration) error {
        d.mu.Lock()
        defer d.mu.Unlock()
        
        if !d.running {
                return fmt.Errorf("debugger not running")
        }
        
        _, err := d.adapter.WaitForStop(int(timeout.Milliseconds()))
        if err != nil {
                return err
        }
        
        d.emitEvent(Event{
                Type:      EventStateChanged,
                Timestamp: time.Now(),
                Data:      d.GetState(),
        })
        
        return nil
}

// === Utility Operations ===

// SetActiveThread sets the active thread
func (d *Debugger) SetActiveThread(threadID int) error {
        d.mu.Lock()
        defer d.mu.Unlock()
        return d.adapter.SetActiveThread(threadID)
}

// SetActiveFrame sets the active stack frame
func (d *Debugger) SetActiveFrame(frameID int) error {
        d.mu.Lock()
        defer d.mu.Unlock()
        return d.adapter.SetActiveFrame(frameID)
}

// GetSource returns source code context
func (d *Debugger) GetSource(file string, startLine, endLine int) ([]string, error) {
        d.mu.RLock()
        defer d.mu.RUnlock()
        
        ctx, err := d.adapter.GetSource(file, startLine, endLine)
        if err != nil {
                return nil, err
        }
        
        lines := make([]string, len(ctx.Lines))
        for i, l := range ctx.Lines {
                lines[i] = l.Content
        }
        return lines, nil
}

// IsRunning checks if the debugger is running
func (d *Debugger) IsRunning() bool {
        d.mu.RLock()
        defer d.mu.RUnlock()
        return d.running
}

// GetAdapterName returns the adapter name
func (d *Debugger) GetAdapterName() string {
        d.mu.RLock()
        defer d.mu.RUnlock()
        return d.adapter.GetName()
}

// GetAdapterVersion returns the adapter version
func (d *Debugger) GetAdapterVersion() string {
        d.mu.RLock()
        defer d.mu.RUnlock()
        return d.adapter.GetVersion()
}

// GetAdapterName returns the adapter name
func (d *Debugger) GetAdapterName() string {
        d.mu.RLock()
        defer d.mu.RUnlock()
        return d.adapter.GetName()
}

// Attach attaches to a running process
func (d *Debugger) Attach(ctx context.Context, pid int) error {
        d.mu.Lock()
        defer d.mu.Unlock()

        if err := d.adapter.Attach(pid); err != nil {
                return fmt.Errorf("failed to attach to process %d: %w", pid, err)
        }

        d.running = true
        d.emitEvent(Event{
                Type:      EventStateChanged,
                Timestamp: time.Now(),
                Data:      d.GetState(),
        })

        return nil
}

// IsRunning returns whether the debugger is running
func (d *Debugger) IsRunning() bool {
        d.mu.RLock()
        defer d.mu.RUnlock()
        return d.running
}

// RemoveBreakpoint removes a breakpoint by ID
func (d *Debugger) RemoveBreakpoint(id int) error {
        d.mu.Lock()
        defer d.mu.Unlock()

        if err := d.adapter.RemoveBreakpoint(id); err != nil {
                return fmt.Errorf("remove breakpoint failed: %w", err)
        }

        d.stateModel.RemoveBreakpoint(id)
        return nil
}

// GetThreads returns all threads
func (d *Debugger) GetThreads() ([]ThreadInfo, error) {
        d.mu.RLock()
        defer d.mu.RUnlock()

        threads, err := d.stateModel.GetAllThreads(100)
        if err != nil {
                return nil, err
        }

        result := make([]ThreadInfo, len(threads))
        for i, t := range threads {
                result[i] = ThreadInfo{
                        ID:     t.ID,
                        Name:   t.Name,
                        State:  t.State.String(),
                        IsMain: t.IsMain,
                }
        }
        return result, nil
}

// SetActiveThread sets the active thread
func (d *Debugger) SetActiveThread(threadID int) error {
        d.mu.Lock()
        defer d.mu.Unlock()
        return d.adapter.SetActiveThread(threadID)
}

// SetActiveFrame sets the active stack frame
func (d *Debugger) SetActiveFrame(frameID int) error {
        d.mu.Lock()
        defer d.mu.Unlock()
        return d.adapter.SetActiveFrame(frameID)
}

// GetSource returns source code lines
func (d *Debugger) GetSource(file string, startLine, endLine int) ([]string, error) {
        d.mu.RLock()
        defer d.mu.RUnlock()

        if startLine <= 0 {
                startLine = 1
        }
        if endLine <= 0 {
                endLine = startLine + 20
        }

        ctx, err := d.adapter.GetSource(file, startLine, endLine)
        if err != nil {
                return nil, err
        }

        lines := make([]string, len(ctx.Lines))
        for i, l := range ctx.Lines {
                lines[i] = l.Content
        }
        return lines, nil
}

// GetStateSummaryJSON returns the state summary as JSON
func (d *Debugger) GetStateSummaryJSON() (string, error) {
        summary := d.GetStateSummary()
        data, err := json.MarshalIndent(summary, "", "  ")
        if err != nil {
                return "", err
        }
        return string(data), nil
}

// GetExecutionContext returns context needed for AI to understand the execution
func (d *Debugger) GetExecutionContext(ctx context.Context) (map[string]interface{}, error) {
        d.mu.RLock()
        defer d.mu.RUnlock()

        result := make(map[string]interface{})

        // Current state
        state := d.GetState()
        result["state"] = state

        // Summary
        summary := d.GetStateSummary()
        result["summary"] = summary

        // Current source context
        if state.CurrentLocation != nil {
                lines, err := d.GetSource(state.CurrentLocation.File,
                        state.CurrentLocation.Line-5,
                        state.CurrentLocation.Line+5)
                if err == nil {
                        result["source_context"] = lines
                }
        }

        return result, nil
}

// GetExecutionContextJSON returns the execution context as JSON
func (d *Debugger) GetExecutionContextJSON(ctx context.Context) (string, error) {
        context, err := d.GetExecutionContext(ctx)
        if err != nil {
                return "", err
        }

        data, err := json.MarshalIndent(context, "", "  ")
        if err != nil {
                return "", err
        }
        return string(data), nil
}
