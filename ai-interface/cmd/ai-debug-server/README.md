# Magic Debugger - AI Interface (Phase 6)

This package provides a Go-based AI interface for the Magic Debugger, enabling AI systems to interact with the debugger programmatically through WebSocket or directly via the Go API.

## Overview

The AI Interface provides:

- **State Reading**: Get current debug state, variables, stack traces
- **Execution Control**: Continue, step over, step into, step out, pause
- **Breakpoint Management**: Set, remove, enable, disable breakpoints
- **Variable Inspection**: Evaluate expressions, get local/global variables
- **AI Helper Functions**: Smart suggestions, code analysis, state summaries
- **WebSocket Server**: Real-time communication for AI integration

## Architecture

```
ai-interface/
├── api/                    # High-level API for AI systems
│   ├── debugger.go         # Main debugger interface
│   └── ai_helper.go        # AI-specific helper functions
├── binding/                # CGO bindings to C library
│   ├── types.go            # Type definitions and conversions
│   ├── types_convert.go    # C to Go type conversions
│   ├── session.go          # Session management bindings
│   ├── state.go            # State model bindings
│   └── adapter.go          # Adapter layer bindings
├── server/                 # WebSocket server
│   └── websocket.go        # WebSocket implementation
├── cmd/                    # Command-line tools
│   └── ai-debug-server/    # AI debug server CLI
└── go.mod                  # Go module definition
```

## Usage

### Starting the AI Debug Server

```bash
# Build the C library first
make

# Build and run the AI server
make ai
make run-ai

# Or directly:
./build/bin/ai-debug-server -port 8765 -debugger lldb -program ./myprogram
```

### WebSocket Protocol

Connect to `ws://localhost:8765/ws` and send JSON messages:

#### Initialize Debugger
```json
{
  "id": 1,
  "type": "initialize"
}
```

#### Launch Program
```json
{
  "id": 2,
  "type": "launch"
}
```

#### Set Breakpoint
```json
{
  "id": 3,
  "type": "set_breakpoint",
  "data": {
    "file": "/path/to/main.c",
    "line": 42,
    "condition": "x > 10"
  }
}
```

#### Continue Execution
```json
{
  "id": 4,
  "type": "continue"
}
```

#### Step Over
```json
{
  "id": 5,
  "type": "step_over"
}
```

#### Get State
```json
{
  "id": 6,
  "type": "get_state"
}
```

#### Get Variables
```json
{
  "id": 7,
  "type": "get_variables",
  "data": {
    "frame_id": 0,
    "scope_type": "locals"
  }
}
```

#### Evaluate Expression
```json
{
  "id": 8,
  "type": "evaluate",
  "data": {
    "expression": "counter + 1",
    "frame_id": 0
  }
}
```

### Go API Usage

```go
package main

import (
    "context"
    "fmt"
    "time"

    "github.com/magic-debugger/ai-interface/api"
)

func main() {
    // Create debugger configuration
    config := api.DefaultConfig()
    config.DebuggerType = api.DebuggerTypeLLDB
    config.ProgramPath = "/path/to/program"
    config.StopOnEntry = true
    
    // Create debugger instance
    debugger, err := api.NewDebugger(config)
    if err != nil {
        panic(err)
    }
    defer debugger.Close()
    
    // Initialize and launch
    ctx := context.Background()
    debugger.Initialize(ctx)
    debugger.Launch(ctx)
    
    // Set breakpoint
    bp, _ := debugger.SetBreakpoint("/path/to/main.c", 42, "")
    fmt.Printf("Breakpoint set: ID=%d\n", bp.ID)
    
    // Continue execution
    debugger.Continue(ctx)
    
    // Wait for stop
    debugger.WaitForStop(ctx, 30*time.Second)
    
    // Get state summary (AI-optimized)
    summary := debugger.GetStateSummary()
    fmt.Printf("State: %s, File: %s:%d\n", 
        summary.ExecutionState, 
        summary.CurrentFile, 
        summary.CurrentLine)
    
    // Get AI suggestions
    suggestions, _ := debugger.SuggestNextAction(ctx)
    for _, s := range suggestions {
        fmt.Printf("Suggestion: %s (confidence: %.2f)\n", s.Reason, s.Confidence)
    }
    
    // Get full execution context for AI
    context, _ := debugger.GetExecutionContextJSON(ctx)
    fmt.Println("Context:", context)
}
```

## AI Helper Functions

### State Summary
Get a condensed state optimized for AI consumption:

```go
summary := debugger.GetStateSummary()
// Returns: program path, execution state, current location,
// stop reason, active breakpoints, threads, stack depth, etc.
```

### Smart Breakpoint Suggestions
Analyze code and suggest optimal breakpoint locations:

```go
suggestions, _ := debugger.SuggestBreakpoints(ctx, "/path/to/main.c")
// Returns suggestions for: function entries, loops, error handling,
// conditional branches, memory allocations, system calls
```

### Next Action Suggestions
Get context-aware suggestions for the next debugging action:

```go
suggestions, _ := debugger.SuggestNextAction(ctx)
// Returns suggestions based on current state:
// - Stopped at breakpoint -> suggest stepping, examining variables
// - Exception occurred -> suggest investigating stack trace
// - Deep call stack -> check for recursion
```

### Code Analysis
Analyze code for debugging insights:

```go
analysis, _ := debugger.AnalyzeCode(ctx, "/path/to/main.c")
// Returns: functions, complexity score, hotspots, suggestions
```

## Event Streaming

Subscribe to debugger events via the Events channel:

```go
events := debugger.Events()
for event := range events {
    switch event.Type {
    case api.EventStateChanged:
        fmt.Println("State changed:", event.Data)
    case api.EventBreakpointHit:
        fmt.Println("Breakpoint hit:", event.Data)
    case api.EventStepComplete:
        fmt.Println("Step completed")
    case api.EventException:
        fmt.Println("Exception:", event.Data)
    }
}
```

## WebSocket Server Options

```bash
ai-debug-server [options]

Options:
  -host string          Server host address (default "0.0.0.0")
  -port int             Server port (default 8765)
  -ws-path string       WebSocket path (default "/ws")
  -allowed-origins      Allowed CORS origins (comma-separated)
  -auth-token string    Authentication token for clients
  -debugger string      Debugger type: lldb, gdb, shell (default "lldb")
  -program string       Program to debug
  -args string          Program arguments (comma-separated)
  -cwd string           Working directory
  -stop-on-entry        Stop at program entry point
  -auto-init            Automatically initialize debugger (default true)
  -timeout int          Operation timeout in milliseconds (default 30000)
  -quiet                Suppress output to stdout
  -version              Show version information
```

## Building

```bash
# Build everything (C library + Go AI interface)
make all
make ai

# Test AI interface
make test-ai

# Install
sudo make install-ai
```

## Requirements

- Go 1.21+
- GCC (for CGO)
- Magic Debugger C library (built in Phase 1-5)

## Integration Examples

### Python Client

```python
import asyncio
import websockets
import json

async def debug_session():
    async with websockets.connect('ws://localhost:8765/ws') as ws:
        # Initialize
        await ws.send(json.dumps({"id": 1, "type": "initialize"}))
        response = json.loads(await ws.recv())
        print("Initialized:", response)
        
        # Launch
        await ws.send(json.dumps({"id": 2, "type": "launch"}))
        response = json.loads(await ws.recv())
        print("Launched:", response)
        
        # Set breakpoint
        await ws.send(json.dumps({
            "id": 3,
            "type": "set_breakpoint",
            "data": {"file": "main.c", "line": 10}
        }))
        response = json.loads(await ws.recv())
        print("Breakpoint:", response)

asyncio.run(debug_session())
```

### LLM Integration

The AI Interface is designed to work seamlessly with LLM-based AI systems:

1. **State Query**: AI can query current debug state
2. **Action Trigger**: AI can trigger step/continue/pause
3. **Variable Access**: AI can read and evaluate variables
4. **Smart Suggestions**: Built-in AI helpers provide context-aware suggestions

Example prompt for AI debugging assistant:

```
You are connected to a debugger via WebSocket. Available commands:
- initialize: Initialize the debugger
- launch: Launch the debuggee program
- get_state: Get current debug state
- continue: Continue execution
- step_over: Step over current line
- set_breakpoint: Set a breakpoint
- evaluate: Evaluate an expression

Current state: {execution_state: "stopped", current_file: "main.c", current_line: 42}
Suggestion: Use "step_over" to continue line by line, or examine local variables.
```

## License

MIT License - See LICENSE file for details.
