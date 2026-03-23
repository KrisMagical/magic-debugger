// Package api provides AI helper functions for intelligent debugging assistance.
// This file implements AI-oriented APIs for smart breakpoint suggestions,
// code analysis, state summaries, and automated debugging workflows.
package api

import (
	"context"
	"encoding/json"
	"fmt"
	"regexp"
	"strings"
	"time"
)

// ============================================================================
// AI Helper Types
// ============================================================================

// AISuggestion represents an AI-generated suggestion
type AISuggestion struct {
	Type        string `json:"type"`         // "breakpoint", "watch", "step", "investigate"
	Confidence  float64 `json:"confidence"`  // 0.0 - 1.0
	Reason      string `json:"reason"`       // Human-readable reason
	Location    *SourceLocation `json:"location,omitempty"`
	Action      string `json:"action,omitempty"` // Suggested action to take
	Priority    int `json:"priority"`      // 1-5, 1 being highest
}

// StateSummary represents a condensed debug state for AI consumption
type StateSummary struct {
	// Basic info
	ProgramPath    string `json:"program_path"`
	ExecutionState string `json:"execution_state"`
	
	// Current position
	CurrentFile    string `json:"current_file,omitempty"`
	CurrentLine    int    `json:"current_line,omitempty"`
	CurrentFunction string `json:"current_function,omitempty"`
	
	// Stop reason
	StopReason     string `json:"stop_reason,omitempty"`
	StopDescription string `json:"stop_description,omitempty"`
	
	// Context summary
	ActiveBreakpoints int `json:"active_breakpoints"`
	ActiveThreads     int `json:"active_threads"`
	StackDepth        int `json:"stack_depth"`
	
	// Variable summary
	LocalVariableCount int `json:"local_variable_count"`
	ModifiedVariables  []string `json:"modified_variables,omitempty"`
	
	// Execution statistics
	TotalSteps         uint64 `json:"total_steps"`
	TotalBreakpointHits uint64 `json:"total_breakpoint_hits"`
	
	// Exception info
	HasException bool   `json:"has_exception"`
	ExceptionType string `json:"exception_type,omitempty"`
	
	// Timestamp
	Timestamp time.Time `json:"timestamp"`
}

// CodeAnalysis represents analysis of source code
type CodeAnalysis struct {
	File          string       `json:"file"`
	Language      string       `json:"language"`
	Functions     []FunctionInfo `json:"functions,omitempty"`
	Complexity    int          `json:"complexity"` // 1-10
	Suggestions   []AISuggestion `json:"suggestions,omitempty"`
	Hotspots      []CodeHotspot `json:"hotspots,omitempty"`
}

// FunctionInfo represents information about a function
type FunctionInfo struct {
	Name       string `json:"name"`
	StartLine  int    `json:"start_line"`
	EndLine    int    `json:"end_line"`
	Parameters []string `json:"parameters,omitempty"`
	IsRecursive bool   `json:"is_recursive"`
}

// CodeHotspot represents a potential problem area in code
type CodeHotspot struct {
	Line        int    `json:"line"`
	Type        string `json:"type"` // "loop", "recursion", "allocation", "io", "error_prone"
	Description string `json:"description"`
	Severity    int    `json:"severity"` // 1-5
}

// ExecutionPattern represents observed execution patterns
type ExecutionPattern struct {
	Type        string `json:"type"` // "loop", "recursion", "branch"
	Count       int    `json:"count"`
	Locations   []SourceLocation `json:"locations,omitempty"`
	AvgDuration time.Duration `json:"avg_duration,omitempty"`
}

// DebugWorkflow represents an automated debugging workflow
type DebugWorkflow struct {
	ID          string   `json:"id"`
	Name        string   `json:"name"`
	Steps       []WorkflowStep `json:"steps"`
	CurrentStep int      `json:"current_step"`
	Status      string   `json:"status"` // "running", "paused", "completed", "failed"
}

// WorkflowStep represents a step in a debugging workflow
type WorkflowStep struct {
	ID          string `json:"id"`
	Name        string `json:"name"`
	Action      string `json:"action"` // "breakpoint", "step", "continue", "evaluate"
	Parameters  map[string]interface{} `json:"parameters,omitempty"`
	Condition   string `json:"condition,omitempty"` // Condition to proceed
	Status      string `json:"status"` // "pending", "running", "completed", "skipped"
	Result      interface{} `json:"result,omitempty"`
}

// ============================================================================
// AI Helper Methods
// ============================================================================

// GetStateSummary returns a condensed summary of the current debug state
// optimized for AI consumption
func (d *Debugger) GetStateSummary() StateSummary {
	d.mu.RLock()
	defer d.mu.RUnlock()

	summary := StateSummary{
		Timestamp: time.Now(),
	}

	if d.debugger == nil {
		return summary
	}

	// Get full state
	state := d.GetState()
	summary.ProgramPath = state.ProgramPath
	summary.ExecutionState = string(state.ExecutionState)
	summary.StopReason = state.StopReason
	summary.StopDescription = state.StopDescription
	summary.TotalSteps = state.TotalSteps
	summary.TotalBreakpointHits = state.TotalBreakpointHits
	summary.HasException = state.Exception != nil

	if state.CurrentLocation != nil {
		summary.CurrentFile = state.CurrentLocation.File
		summary.CurrentLine = state.CurrentLocation.Line
	}

	// Count breakpoints
	summary.ActiveBreakpoints = len(state.Breakpoints)

	// Count threads
	summary.ActiveThreads = len(state.Threads)

	// Stack depth
	summary.StackDepth = len(state.StackTrace)
	if summary.StackDepth > 0 && len(state.StackTrace) > 0 {
		summary.CurrentFunction = state.StackTrace[0].Function
	}

	// Local variables
	summary.LocalVariableCount = len(state.LocalVariables)

	// Exception
	if state.Exception != nil {
		summary.ExceptionType = state.Exception.Type
	}

	return summary
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

// SuggestBreakpoints analyzes code and suggests optimal breakpoint locations
func (d *Debugger) SuggestBreakpoints(ctx context.Context, file string) ([]AISuggestion, error) {
	d.mu.RLock()
	defer d.mu.RUnlock()

	var suggestions []AISuggestion

	// Get source code
	lines, err := d.GetSource(file, 1, 500)
	if err != nil {
		return nil, fmt.Errorf("failed to read source: %w", err)
	}

	// Analyze for potential breakpoint locations
	for i, line := range lines {
		lineNum := i + 1
		trimmed := strings.TrimSpace(line)

		// Skip empty lines and comments
		if trimmed == "" || strings.HasPrefix(trimmed, "//") || strings.HasPrefix(trimmed, "/*") {
			continue
		}

		// Function entry points
		if isFunctionEntry(trimmed) {
			suggestions = append(suggestions, AISuggestion{
				Type:       "breakpoint",
				Confidence: 0.9,
				Reason:     "Function entry point - useful for tracking function calls",
				Location:   &SourceLocation{File: file, Line: lineNum},
				Priority:   2,
			})
		}

		// Loop constructs
		if isLoopConstruct(trimmed) {
			suggestions = append(suggestions, AISuggestion{
				Type:       "breakpoint",
				Confidence: 0.85,
				Reason:     "Loop construct - useful for debugging iteration issues",
				Location:   &SourceLocation{File: file, Line: lineNum},
				Priority:   3,
			})
		}

		// Error handling
		if isErrorHandling(trimmed) {
			suggestions = append(suggestions, AISuggestion{
				Type:       "breakpoint",
				Confidence: 0.95,
				Reason:     "Error handling code - useful for debugging error conditions",
				Location:   &SourceLocation{File: file, Line: lineNum},
				Priority:   1,
			})
		}

		// Conditional branches
		if isConditionalBranch(trimmed) {
			suggestions = append(suggestions, AISuggestion{
				Type:       "breakpoint",
				Confidence: 0.8,
				Reason:     "Conditional branch - useful for tracking control flow",
				Location:   &SourceLocation{File: file, Line: lineNum},
				Priority:   3,
			})
		}

		// Memory allocations
		if isMemoryAllocation(trimmed) {
			suggestions = append(suggestions, AISuggestion{
				Type:       "watch",
				Confidence: 0.75,
				Reason:     "Memory allocation - consider watching for memory issues",
				Location:   &SourceLocation{File: file, Line: lineNum},
				Priority:   4,
			})
		}

		// System calls / IO
		if isSystemCall(trimmed) {
			suggestions = append(suggestions, AISuggestion{
				Type:       "breakpoint",
				Confidence: 0.7,
				Reason:     "System call / IO operation - useful for debugging external interactions",
				Location:   &SourceLocation{File: file, Line: lineNum},
				Priority:   4,
			})
		}
	}

	// Sort by priority (lower number = higher priority)
	sortSuggestions(suggestions)

	return suggestions, nil
}

// AnalyzeCode performs comprehensive code analysis for debugging
func (d *Debugger) AnalyzeCode(ctx context.Context, file string) (*CodeAnalysis, error) {
	d.mu.RLock()
	defer d.mu.RUnlock()

	analysis := &CodeAnalysis{
		File:    file,
		Language: detectLanguage(file),
	}

	// Get source code
	lines, err := d.GetSource(file, 1, 1000)
	if err != nil {
		return nil, fmt.Errorf("failed to read source: %w", err)
	}

	// Extract functions
	analysis.Functions = extractFunctions(lines)

	// Calculate complexity
	analysis.Complexity = calculateComplexity(lines)

	// Generate suggestions
	suggestions, _ := d.SuggestBreakpoints(ctx, file)
	analysis.Suggestions = suggestions

	// Find hotspots
	analysis.Hotspots = findHotspots(lines)

	return analysis, nil
}

// SuggestNextAction suggests the next debugging action based on current state
func (d *Debugger) SuggestNextAction(ctx context.Context) ([]AISuggestion, error) {
	d.mu.RLock()
	defer d.mu.RUnlock()

	var suggestions []AISuggestion

	state := d.GetState()
	summary := d.GetStateSummary()

	switch state.ExecutionState {
	case StateStopped:
		// Program is stopped, analyze why
		switch state.StopReason {
		case "breakpoint":
			suggestions = append(suggestions, AISuggestion{
				Type:       "step",
				Confidence: 0.9,
				Reason:     "Breakpoint hit - step into to examine function behavior",
				Action:     "step_into",
				Priority:   1,
			})
			suggestions = append(suggestions, AISuggestion{
				Type:       "step",
				Confidence: 0.85,
				Reason:     "Breakpoint hit - step over to continue line by line",
				Action:     "step_over",
				Priority:   2,
			})
			suggestions = append(suggestions, AISuggestion{
				Type:       "evaluate",
				Confidence: 0.8,
				Reason:     "Examine local variables at breakpoint",
				Action:     "get_variables",
				Priority:   1,
			})

		case "step":
			suggestions = append(suggestions, AISuggestion{
				Type:       "step",
				Confidence: 0.9,
				Reason:     "Step completed - continue stepping to trace execution",
				Action:     "step_over",
				Priority:   1,
			})
			suggestions = append(suggestions, AISuggestion{
				Type:       "step",
				Confidence: 0.7,
				Reason:     "Step completed - continue execution to next breakpoint",
				Action:     "continue",
				Priority:   3,
			})

		case "exception":
			suggestions = append(suggestions, AISuggestion{
				Type:       "investigate",
				Confidence: 0.95,
				Reason:     "Exception occurred - examine stack trace and variables",
				Action:     "get_stack_trace",
				Priority:   1,
			})
			suggestions = append(suggestions, AISuggestion{
				Type:       "evaluate",
				Confidence: 0.9,
				Reason:     "Exception occurred - examine exception details",
				Action:     "get_exception",
				Priority:   1,
			})

		case "entry":
			suggestions = append(suggestions, AISuggestion{
				Type:       "step",
				Confidence: 0.9,
				Reason:     "Program stopped at entry point - begin stepping",
				Action:     "step_over",
				Priority:   1,
			})
			suggestions = append(suggestions, AISuggestion{
				Type:       "breakpoint",
				Confidence: 0.85,
				Reason:     "Set breakpoints at key locations before continuing",
				Action:     "suggest_breakpoints",
				Priority:   2,
			})
		}

	case StateRunning:
		suggestions = append(suggestions, AISuggestion{
			Type:       "step",
			Confidence: 0.8,
			Reason:     "Program running - pause to inspect state",
			Action:     "pause",
			Priority:   2,
		})

	case StateTerminated:
		suggestions = append(suggestions, AISuggestion{
			Type:       "step",
			Confidence: 0.9,
			Reason:     "Program terminated - restart to debug again",
			Action:     "restart",
			Priority:   1,
		})

	case StateCrashed:
		suggestions = append(suggestions, AISuggestion{
			Type:       "investigate",
			Confidence: 0.95,
			Reason:     "Program crashed - analyze crash location and state",
			Action:     "get_state",
			Priority:   1,
		})
	}

	// Add context-based suggestions
	if summary.StackDepth > 10 {
		suggestions = append(suggestions, AISuggestion{
			Type:       "investigate",
			Confidence: 0.85,
			Reason:     "Deep call stack detected - check for infinite recursion",
			Action:     "get_stack_trace",
			Priority:   2,
		})
	}

	if summary.ActiveBreakpoints == 0 {
		suggestions = append(suggestions, AISuggestion{
			Type:       "breakpoint",
			Confidence: 0.8,
			Reason:     "No breakpoints set - consider adding breakpoints for debugging",
			Action:     "suggest_breakpoints",
			Priority:   3,
		})
	}

	sortSuggestions(suggestions)
	return suggestions, nil
}

// WatchExpression creates a watched expression that triggers on value change
type WatchExpression struct {
	ID          string `json:"id"`
	Expression  string `json:"expression"`
	LastValue   string `json:"last_value"`
	LastChecked time.Time `json:"last_checked"`
	ChangeCount int    `json:"change_count"`
}

// CreateWatch creates a watch on an expression
func (d *Debugger) CreateWatch(expression string, frameID int) (*WatchExpression, error) {
	d.mu.Lock()
	defer d.mu.Unlock()

	// Evaluate the expression
	result, err := d.Evaluate(expression, frameID)
	if err != nil {
		return nil, fmt.Errorf("failed to evaluate expression: %w", err)
	}

	watch := &WatchExpression{
		ID:          generateWatchID(expression),
		Expression:  expression,
		LastValue:   result.Value,
		LastChecked: time.Now(),
		ChangeCount: 0,
	}

	return watch, nil
}

// CheckWatch checks if a watched expression has changed
func (d *Debugger) CheckWatch(watchID string, frameID int) (*WatchExpression, bool, error) {
	d.mu.RLock()
	defer d.mu.RUnlock()

	// Find the watch
	// Note: In a full implementation, we'd store watches
	// For now, we'll just evaluate and return
	return nil, false, fmt.Errorf("watch not found: %s", watchID)
}

// QuickDebug performs a quick debug session with common operations
func (d *Debugger) QuickDebug(ctx context.Context, programPath string, stopAtMain bool) error {
	// Initialize
	if err := d.Initialize(ctx); err != nil {
		return fmt.Errorf("failed to initialize: %w", err)
	}

	// Launch
	if err := d.Launch(ctx); err != nil {
		return fmt.Errorf("failed to launch: %w", err)
	}

	// If stop at main, set breakpoint at main and continue
	if stopAtMain {
		_, err := d.SetBreakpoint(programPath, 0, "") // Line 0 = function entry
		if err == nil {
			d.Continue(ctx)
		}
	}

	return nil
}

// GetExecutionContext returns context needed for AI to understand the execution
func (d *Debugger) GetExecutionContext(ctx context.Context) (map[string]interface{}, error) {
	d.mu.RLock()
	defer d.mu.RUnlock()

	context := make(map[string]interface{})

	// Current state
	state := d.GetState()
	context["state"] = state

	// Summary
	summary := d.GetStateSummary()
	context["summary"] = summary

	// Suggestions
	suggestions, _ := d.SuggestNextAction(ctx)
	context["suggestions"] = suggestions

	// Current source context
	if state.CurrentLocation != nil {
		lines, err := d.GetSource(state.CurrentLocation.File, 
			state.CurrentLocation.Line - 5, 
			state.CurrentLocation.Line + 5)
		if err == nil {
			context["source_context"] = lines
		}
	}

	return context, nil
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

// ============================================================================
// Helper Functions
// ============================================================================

func sortSuggestions(suggestions []AISuggestion) {
	// Sort by priority (lower = higher priority)
	for i := 0; i < len(suggestions)-1; i++ {
		for j := i + 1; j < len(suggestions); j++ {
			if suggestions[j].Priority < suggestions[i].Priority {
				suggestions[i], suggestions[j] = suggestions[j], suggestions[i]
			}
		}
	}
}

func isFunctionEntry(line string) bool {
	// C/C++ style
	if matched, _ := regexp.MatchString(`^\w+\s+\w+\s*\([^)]*\)\s*\{?\s*$`, line); matched {
		return true
	}
	// Go style
	if matched, _ := regexp.MatchString(`^func\s+\w+\s*\([^)]*\)`, line); matched {
		return true
	}
	// Python style
	if matched, _ := regexp.MatchString(`^def\s+\w+\s*\([^)]*\)\s*:`, line); matched {
		return true
	}
	return false
}

func isLoopConstruct(line string) bool {
	lower := strings.ToLower(line)
	return strings.Contains(lower, "for ") ||
		strings.Contains(lower, "while ") ||
		strings.Contains(lower, "do ") ||
		strings.Contains(lower, "foreach")
}

func isErrorHandling(line string) bool {
	lower := strings.ToLower(line)
	return strings.Contains(lower, "catch") ||
		strings.Contains(lower, "except") ||
		strings.Contains(lower, "error") ||
		strings.Contains(lower, "panic") ||
		strings.Contains(lower, "recover") ||
		strings.Contains(lower, "throw") ||
		strings.Contains(lower, "raise")
}

func isConditionalBranch(line string) bool {
	lower := strings.ToLower(line)
	return strings.HasPrefix(lower, "if ") ||
		strings.HasPrefix(lower, "else if") ||
		strings.HasPrefix(lower, "elif ") ||
		strings.HasPrefix(lower, "switch ") ||
		strings.HasPrefix(lower, "case ")
}

func isMemoryAllocation(line string) bool {
	return strings.Contains(line, "malloc") ||
		strings.Contains(line, "calloc") ||
		strings.Contains(line, "realloc") ||
		strings.Contains(line, "free(") ||
		strings.Contains(line, "new ") ||
		strings.Contains(line, "delete ") ||
		strings.Contains(line, "make(") ||
		strings.Contains(line, "alloc")
}

func isSystemCall(line string) bool {
	lower := strings.ToLower(line)
	return strings.Contains(lower, "open(") ||
		strings.Contains(lower, "read(") ||
		strings.Contains(lower, "write(") ||
		strings.Contains(lower, "close(") ||
		strings.Contains(lower, "socket") ||
		strings.Contains(lower, "connect") ||
		strings.Contains(lower, "bind(") ||
		strings.Contains(lower, "listen(") ||
		strings.Contains(lower, "accept(") ||
		strings.Contains(lower, "fopen") ||
		strings.Contains(lower, "fclose") ||
		strings.Contains(lower, "fprintf") ||
		strings.Contains(lower, "printf")
}

func detectLanguage(file string) string {
	if strings.HasSuffix(file, ".c") {
		return "c"
	}
	if strings.HasSuffix(file, ".cpp") || strings.HasSuffix(file, ".cc") || strings.HasSuffix(file, ".cxx") {
		return "cpp"
	}
	if strings.HasSuffix(file, ".go") {
		return "go"
	}
	if strings.HasSuffix(file, ".py") {
		return "python"
	}
	if strings.HasSuffix(file, ".rs") {
		return "rust"
	}
	if strings.HasSuffix(file, ".java") {
		return "java"
	}
	if strings.HasSuffix(file, ".sh") || strings.HasSuffix(file, ".bash") {
		return "shell"
	}
	return "unknown"
}

func extractFunctions(lines []string) []FunctionInfo {
	var functions []FunctionInfo

	for i, line := range lines {
		if isFunctionEntry(line) {
			name := extractFunctionName(line)
			if name != "" {
				functions = append(functions, FunctionInfo{
					Name:      name,
					StartLine: i + 1,
					EndLine:   findFunctionEnd(lines, i),
				})
			}
		}
	}

	return functions
}

func extractFunctionName(line string) string {
	// Go func
	if strings.HasPrefix(strings.TrimSpace(line), "func ") {
		re := regexp.MustCompile(`func\s+(\w+)`)
		matches := re.FindStringSubmatch(line)
		if len(matches) > 1 {
			return matches[1]
		}
	}

	// C/C++ style
	re := regexp.MustCompile(`\b(\w+)\s*\([^)]*\)\s*\{?`)
	matches := re.FindStringSubmatch(line)
	if len(matches) > 1 {
		return matches[1]
	}

	return ""
}

func findFunctionEnd(lines []string, startIdx int) int {
	braceCount := 0
	started := false

	for i := startIdx; i < len(lines); i++ {
		for _, ch := range lines[i] {
			if ch == '{' {
				braceCount++
				started = true
			} else if ch == '}' {
				braceCount--
				if started && braceCount == 0 {
					return i + 1
				}
			}
		}
	}

	return len(lines)
}

func calculateComplexity(lines []string) int {
	complexity := 1

	for _, line := range lines {
		lower := strings.ToLower(line)

		// Add complexity for control structures
		if strings.Contains(lower, "if ") {
			complexity++
		}
		if strings.Contains(lower, "else if") || strings.Contains(lower, "elif") {
			complexity++
		}
		if strings.Contains(lower, "for ") || strings.Contains(lower, "while ") {
			complexity += 2
		}
		if strings.Contains(lower, "switch ") || strings.Contains(lower, "case ") {
			complexity++
		}
		if strings.Contains(lower, "catch") || strings.Contains(lower, "except") {
			complexity++
		}
	}

	// Normalize to 1-10 scale
	if complexity > 10 {
		complexity = 10
	}
	return complexity
}

func findHotspots(lines []string) []CodeHotspot {
	var hotspots []CodeHotspot

	for i, line := range lines {
		lineNum := i + 1

		// Nested loops
		if isLoopConstruct(line) {
			nesting := countNesting(lines, i)
			if nesting > 1 {
				hotspots = append(hotspots, CodeHotspot{
					Line:        lineNum,
					Type:        "loop",
					Description: fmt.Sprintf("Nested loop (depth %d)", nesting),
					Severity:    nesting,
				})
			}
		}

		// Memory allocations
		if isMemoryAllocation(line) {
			hotspots = append(hotspots, CodeHotspot{
				Line:        lineNum,
				Type:        "allocation",
				Description: "Memory allocation",
				Severity:    2,
			})
		}

		// System calls
		if isSystemCall(line) {
			hotspots = append(hotspots, CodeHotspot{
				Line:        lineNum,
				Type:        "io",
				Description: "IO operation",
				Severity:    1,
			})
		}

		// Recursion potential
		if isFunctionEntry(line) {
			name := extractFunctionName(line)
			if name != "" && isRecursiveCall(lines, i, name) {
				hotspots = append(hotspots, CodeHotspot{
					Line:        lineNum,
					Type:        "recursion",
					Description: "Potential recursive function",
					Severity:    3,
				})
			}
		}
	}

	return hotspots
}

func countNesting(lines []string, idx int) int {
	nesting := 0
	for i := 0; i <= idx; i++ {
		if isLoopConstruct(lines[i]) || isConditionalBranch(lines[i]) {
			nesting++
		}
		// Check for closing braces to reduce nesting
		for _, ch := range lines[i] {
			if ch == '}' {
				nesting--
				if nesting < 0 {
					nesting = 0
				}
			}
		}
	}
	return nesting
}

func isRecursiveCall(lines []string, funcStart int, funcName string) bool {
	// Look for function calls within the function body
	for i := funcStart + 1; i < len(lines); i++ {
		line := lines[i]
		if strings.Contains(line, funcName+"(") {
			return true
		}
		// Stop at function boundary
		if strings.Contains(line, "}") && countBraces(lines[funcStart:i+1]) == 0 {
			break
		}
	}
	return false
}

func countBraces(lines []string) int {
	count := 0
	for _, line := range lines {
		for _, ch := range line {
			if ch == '{' {
				count++
			} else if ch == '}' {
				count--
			}
		}
	}
	return count
}

func generateWatchID(expression string) string {
	return fmt.Sprintf("watch_%d", time.Now().UnixNano())
}
