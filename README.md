# Magic Debugger

A DAP (Debug Adapter Protocol) based unified debugging platform.

## Overview

Magic Debugger is designed to provide a unified interface for multiple debuggers (gdb/lldb/shell) with support for TUI, Vim, and AI integration.

## Completed Phases

### Phase 1: Session Manager

The Session Manager is the foundation of Magic Debugger, providing:

- **Process Creation**: Fork + exec for spawning debugger processes
- **Pipe Communication**: stdin/stdout redirection for communication
- **Non-blocking I/O**: Using select()/poll() for async operations
- **Session Lifecycle Management**: Creation, monitoring, and cleanup

### Phase 2: DAP Client

The DAP Client provides communication with debug adapters:

- **Message Protocol**: Content-Length header + JSON body format
- **Request/Response Handling**: Sequence number tracking
- **Event Dispatching**: Callback-based event handling
- **Core Commands**: initialize, launch, setBreakpoints, continue, next, stepIn, stepOut

### Phase 3: State Model

The State Model provides unified debug state representation:

- **Execution State**: Running, stopped, terminated states
- **Breakpoint Management**: Add, remove, enable, disable breakpoints
- **Thread Management**: Thread list and state tracking
- **Stack Frames**: Call stack representation
- **Variables**: Variable scopes and values
- **DAP Event Processing**: Parse and handle DAP events

### Phase 4: Adapter Layer

The Adapter Layer provides debugger-specific implementations:

- **LLDB Adapter**: Integration with lldb-dap for C/C++/Rust debugging
- **GDB Adapter**: Integration with GDB/MI for C/C++ debugging
- **Shell Adapter**: Integration with bashdb for shell script debugging

Key Features:
- **Unified Breakpoint Management**: Set, remove, enable/disable breakpoints
- **Execution Control**: Continue, step over, step into, step out
- **Variable Inspection**: Evaluate expressions, examine/modify variables
- **Source Display**: Show source code with breakpoint markers
- **Thread/Stack Management**: Navigate threads and call stacks

### Phase 5: Frontend (TUI and Vim Plugin)

The Frontend provides user interfaces for interactive debugging:

#### TUI (Terminal User Interface)

A full-featured terminal interface using ncurses:

- **Multi-Panel Layout**: Source, breakpoints, call stack, variables, output
- **Source View**: Syntax highlighting, breakpoint markers, current line indicator
- **Breakpoint Panel**: List, enable, disable, navigate breakpoints
- **Call Stack Panel**: Navigate stack frames, jump to source locations
- **Variable Panel**: Inspect variables, expand complex types
- **Output Panel**: Debug output, command history
- **Thread Panel**: View and switch between threads

Key Bindings (TUI):
| Key | Action |
|-----|--------|
| F5 / c | Continue execution |
| F6 / n | Step over |
| F7 / s | Step into |
| F8 / f | Step out |
| F9 / b | Toggle breakpoint |
| r | Restart debuggee |
| q | Quit debugger |
| ? | Show help |
| : | Enter command mode |
| H/L | Previous/next panel |

#### Vim Plugin

Full integration with Vim/Neovim:

- **Sign-based Breakpoints**: Visual breakpoint markers in the sign column
- **Current Line Highlight**: Highlight current execution position
- **Debug Windows**: Stack, variables, output in split windows
- **Command Mode**: `:MagicDebug*` commands for all operations
- **Key Mappings**: Standard debugger key bindings
- **Async Communication**: Non-blocking communication with debugger

Vim Commands:
```
:MagicDebugStart [program]     - Start debugging session
:MagicDebugStop                - Stop debugging session
:MagicDebugContinue            - Continue execution
:MagicDebugNext                - Step over
:MagicDebugStep                - Step into
:MagicDebugFinish              - Step out
:MagicDebugToggleBreakpoint    - Toggle breakpoint
:MagicDebugEval <expr>         - Evaluate expression
:MagicDebugWatch <expr>        - Add watch expression
:MagicDebugToggleUI            - Toggle debug windows
```

Vim Key Mappings (default prefix `<Leader>d`):
| Key | Action |
|-----|--------|
| `<Leader>ds` | Start debugging |
| `<Leader>dx` | Stop debugging |
| `<Leader>dc` | Continue |
| `<Leader>dn` | Step over |
| `<Leader>di` | Step into |
| `<Leader>do` | Step out |
| `<Leader>db` | Toggle breakpoint |
| `<Leader>dt` | Toggle UI |
| F5 | Continue |
| F6 | Step over |
| F7 | Step into |
| F8 | Step out |
| F9 | Toggle breakpoint |

### Architecture

```
┌───────────────┐
│     AI        │
└──────┬────────┘
       │ JSON API
       ▼
┌──────────────────────────────────────────┐
│         Magic Debug Core (C)             │
│ ┌──────────────┐  ┌──────────────────┐  │
│ │ Session Mgr  │  │   State Model    │  │
│ └──────┬───────┘  └────────┬─────────┘  │
│        │                    │            │
│ ┌──────▼────────┐  ┌────────▼────────┐  │
│ │  DAP Client   │  │ Adapter Layer   │  │
│ └──────┬────────┘  └────────┬────────┘  │
└────────┼────────────────────┼───────────┘
         │                    │
         ▼                    ▼
   lldb-dap / gdb-dap    shell adapter

┌──────────────────────────────────────────┐
│           Frontend Layer                  │
│ ┌──────────────┐  ┌──────────────────┐  │
│ │   TUI (C)    │  │   Vim Plugin     │  │
│ │  (ncurses)   │  │    (VimScript)   │  │
│ └──────────────┘  └──────────────────┘  │
└──────────────────────────────────────────┘
```

## Building

### Prerequisites

- GCC or Clang with C11 support
- POSIX-compliant system (Linux, macOS)
- pthread library
- ncurses library (for TUI)
- Vim 8.0+ or Neovim (for Vim plugin)

### Build with Make

```bash
# Build all (libraries and tests)
make all

# Run all tests
make test

# Run specific test suites
make test-session     # Phase 1-2 tests
make test-state-model # Phase 3 tests
make test-adapter     # Phase 4 tests
make test-tui         # Phase 5 tests

# Clean
make clean

# Install (requires root for libraries)
make install          # Install everything
make install-lib      # Install libraries only
make install-vim      # Install Vim plugin only
```

### Build with CMake

```bash
mkdir build && cd build
cmake ..
make
make test
```

## Project Structure

```
magic-debugger/
├── include/
│   ├── types.h            # Common types and macros
│   ├── logger.h           # Logging system
│   ├── io_handler.h       # Non-blocking I/O
│   ├── signal_handler.h   # Signal handling
│   ├── session_manager.h  # Session management
│   ├── dap_types.h        # DAP type definitions
│   ├── dap_protocol.h     # DAP protocol constants
│   ├── dap_client.h       # DAP client API
│   ├── state_types.h      # State type definitions
│   ├── state_model.h      # State model API
│   ├── adapter.h          # Unified adapter interface
│   ├── lldb_adapter.h     # LLDB adapter
│   ├── gdb_adapter.h      # GDB adapter
│   ├── shell_adapter.h    # Shell adapter
│   ├── tui.h              # TUI core interface
│   ├── tui_panel.h        # TUI panel definitions
│   └── cJSON.h            # JSON parser
├── src/
│   ├── types.c
│   ├── logger.c
│   ├── io_handler.c
│   ├── signal_handler.c
│   ├── session_manager.c
│   ├── dap_types.c
│   ├── dap_client.c
│   ├── state_types.c
│   ├── state_model.c
│   ├── adapter.c
│   ├── lldb_adapter.c
│   ├── gdb_adapter.c
│   ├── shell_adapter.c
│   ├── tui.c              # TUI implementation
│   ├── tui_panel.c        # Panel implementations
│   └── cJSON.c
├── vim/
│   ├── plugin/
│   │   └── magic_debugger.vim     # Plugin entry point
│   └── autoload/
│       ├── magic_debugger.vim     # Core functionality
│       ├── magic_debugger/
│       │   ├── ui.vim             # UI management
│       │   └── cmd.vim            # Command handling
├── ai-interface/                  # Phase 6: AI Interface (Go)
│   ├── api/
│   │   ├── debugger.go            # Main debugger API
│   │   ├── ai_helper.go           # AI helper functions
│   │   └── types.go               # API types
│   ├── binding/                   # CGO bindings
│   │   ├── types.go               # Type definitions
│   │   ├── types_convert.go       # Type conversions
│   │   ├── session.go             # Session bindings
│   │   ├── state.go               # State model bindings
│   │   └── adapter.go             # Adapter bindings
│   ├── server/
│   │   └── websocket.go           # WebSocket server
│   ├── cmd/
│   │   └── ai-debug-server/       # CLI tool
│   └── README.md                  # AI Interface documentation
├── test/
│   ├── test_session.c     # Phase 1-2 tests
│   ├── test_state_model.c # Phase 3 tests
│   ├── test_adapter.c     # Phase 4 tests
│   └── test_tui.c         # Phase 5 tests
├── Makefile
└── CMakeLists.txt
```

## API Usage

### Session Manager

```c
#include <session_manager.h>

// Create manager
md_session_manager_t *manager = md_manager_create(NULL);

// Configure session
md_session_config_t config;
md_session_config_init(&config);
md_session_config_set_debugger(&config, "/usr/bin/lldb-dap");
md_session_config_set_program(&config, "./myprogram");

// Create session
md_session_t *session = md_session_create(manager, &config);

// Send data
md_session_send_string(session, "help\n", NULL);

// Receive data
char buffer[1024];
size_t bytes_read;
md_session_receive(session, buffer, sizeof(buffer), &bytes_read);

// Cleanup
md_session_destroy(session);
md_manager_destroy(manager);
```

### State Model

```c
#include <state_model.h>

// Create state model
md_state_model_t *model = md_state_model_create(NULL);

// Set execution state
md_state_set_exec_state(model, MD_EXEC_STATE_STOPPED);
md_state_set_stop_reason(model, MD_STOP_REASON_BREAKPOINT, "Breakpoint hit");

// Add breakpoint
md_breakpoint_t bp;
md_breakpoint_init(&bp);
strcpy(bp.path, "/src/main.c");
bp.line = 42;
int bp_id = md_state_add_breakpoint(model, &bp);

// Add thread
md_thread_t thread;
md_thread_init(&thread);
thread.id = 1;
strcpy(thread.name, "main");
thread.state = MD_THREAD_STATE_STOPPED;
md_state_add_thread(model, &thread);

// Set stack frames
md_stack_frame_t frames[3];
for (int i = 0; i < 3; i++) {
    md_stack_frame_init(&frames[i]);
    frames[i].id = i;
    frames[i].line = 10 + i;
}
md_state_set_stack_frames(model, 1, frames, 3);

// Get debug state snapshot
md_debug_state_t state;
md_state_get_debug_state(model, &state);
printf("Exec state: %s\n", md_exec_state_string(state.exec_state));

// Cleanup
md_state_model_destroy(model);
```

### Adapter Layer

```c
#include <adapter.h>

// Create adapter configuration
md_adapter_config_t config;
md_adapter_config_init(&config);
md_adapter_config_set_debugger(&config, "/usr/bin/lldb-dap");
md_adapter_config_set_program(&config, "./myprogram");

// Create LLDB adapter
md_adapter_t *adapter = md_adapter_create(MD_DEBUGGER_LLDB, &config);

// Initialize and launch
md_adapter_init(adapter);
md_adapter_launch(adapter);

// Set breakpoint
int bp_id;
md_adapter_set_breakpoint(adapter, "/src/main.c", 42, NULL, &bp_id);

// Continue execution
md_adapter_continue(adapter);

// Wait for stop
md_stop_reason_t reason;
md_adapter_wait_for_stop(adapter, 5000, &reason);

// Get stack frames
md_stack_frame_t frames[10];
int frame_count;
md_adapter_get_stack_frames(adapter, 1, frames, 10, &frame_count);

// Cleanup
md_adapter_disconnect(adapter, true);
md_adapter_destroy(adapter);
```

### TUI

```c
#include <tui.h>
#include <tui_panel.h>

// Create TUI configuration
md_tui_config_t config;
md_tui_config_init(&config);

// Create TUI context
md_tui_t *tui = md_tui_create(adapter, &config);

// Initialize ncurses
md_tui_init(tui);

// Create standard panels
md_tui_create_standard_panels(tui);

// Run main loop
md_tui_run(tui);

// Cleanup
md_tui_destroy(tui);
```

## Development Phases

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Session Manager | ✅ Complete |
| 2 | DAP Client | ✅ Complete |
| 3 | State Model | ✅ Complete |
| 4 | Adapter Layer | ✅ Complete |
| 5 | Frontend (TUI/Vim) | ✅ Complete |
| 6 | AI Interface (Go) | ✅ Complete |

### Phase 6: AI Interface (Go)

The AI Interface provides a Go-based API for AI systems to interact with the debugger:

#### Key Features

- **State Reading**: AI can read current debug state, variables, stack traces
- **Execution Control**: AI can trigger continue, step over/into/out, pause
- **Breakpoint Management**: AI can set, remove, enable, disable breakpoints
- **Variable Inspection**: AI can evaluate expressions, inspect variables
- **Smart Suggestions**: Built-in AI helpers for context-aware suggestions
- **WebSocket Server**: Real-time communication for AI integration

#### Architecture

```
┌─────────────────────────────────────────┐
│           AI System / LLM               │
└─────────────────┬───────────────────────┘
                  │ WebSocket / Go API
                  ▼
┌─────────────────────────────────────────┐
│         AI Interface (Go)               │
│ ┌───────────────┐  ┌─────────────────┐  │
│ │  API Layer    │  │ WebSocket Srv   │  │
│ │ (debugger.go) │  │ (websocket.go)  │  │
│ └───────┬───────┘  └─────────────────┘  │
│         │                               │
│ ┌───────▼───────────────────────────┐  │
│ │        CGO Binding Layer          │  │
│ │  (session.go, state.go, adapter.go)│  │
│ └─────────────────┬─────────────────┘  │
└───────────────────┼─────────────────────┘
                    │ CGO
                    ▼
         ┌─────────────────────┐
         │   C Library         │
         │ (libmagic_debugger) │
         └─────────────────────┘
```

#### Usage (Go API)

```go
import "github.com/magic-debugger/ai-interface/api"

func main() {
    config := api.DefaultConfig()
    config.DebuggerType = api.DebuggerTypeLLDB
    config.ProgramPath = "/path/to/program"
    
    debugger, _ := api.NewDebugger(config)
    defer debugger.Close()
    
    // Initialize and launch
    debugger.Initialize(ctx)
    debugger.Launch(ctx)
    
    // Set breakpoint
    debugger.SetBreakpoint("/path/to/main.c", 42, "")
    
    // Get AI-optimized state summary
    summary := debugger.GetStateSummary()
    
    // Get next action suggestions
    suggestions, _ := debugger.SuggestNextAction(ctx)
    
    // Step execution
    debugger.StepOver(ctx)
}
```

#### WebSocket Protocol

Connect to `ws://localhost:8765/ws` and send JSON messages:

```json
{"id": 1, "type": "initialize"}
{"id": 2, "type": "launch"}
{"id": 3, "type": "set_breakpoint", "data": {"file": "main.c", "line": 10}}
{"id": 4, "type": "continue"}
{"id": 5, "type": "get_state"}
```

#### Building AI Interface

```bash
# Build C library first
make

# Build AI interface
make ai

# Run AI debug server
make run-ai
# or
./build/bin/ai-debug-server -port 8765 -debugger lldb -program ./myprogram
```

#### AI Helper Functions

```go
// Get condensed state for AI
summary := debugger.GetStateSummary()

// Get smart breakpoint suggestions
suggestions, _ := debugger.SuggestBreakpoints(ctx, "/path/to/main.c")

// Get context-aware next action suggestions
actions, _ := debugger.SuggestNextAction(ctx)

// Get full execution context for AI analysis
context, _ := debugger.GetExecutionContextJSON(ctx)
```

See [ai-interface/README.md](ai-interface/README.md) for detailed documentation.

## License

MIT License

## Contributing

Contributions are welcome! Please read the contributing guidelines before submitting PRs.
