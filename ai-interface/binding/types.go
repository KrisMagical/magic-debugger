// Package binding provides CGO bindings to the Magic Debugger C library.
// This package exposes the C API to Go, enabling AI systems to interact
// with the debugger programmatically.
package binding

/*
#cgo CFLAGS: -I${SRCDIR}/../../include -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
#cgo LDFLAGS: -L${SRCDIR}/../../build -lmagic_debugger -lpthread

#include <types.h>
#include <logger.h>
#include <session_manager.h>
#include <state_types.h>
#include <state_model.h>
#include <adapter.h>
#include <dap_types.h>
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>
*/
import "C"
import (
	"errors"
	"fmt"
	"runtime"
	"unsafe"
)

// Error codes from C library
var (
	ErrSuccess              = errors.New("success")
	ErrUnknown              = errors.New("unknown error")
	ErrInvalidArg           = errors.New("invalid argument")
	ErrNullPointer          = errors.New("null pointer")
	ErrOutOfMemory          = errors.New("out of memory")
	ErrPipeFailed           = errors.New("pipe creation failed")
	ErrForkFailed           = errors.New("fork failed")
	ErrExecFailed           = errors.New("exec failed")
	ErrSessionNotFound      = errors.New("session not found")
	ErrSessionExists        = errors.New("session already exists")
	ErrInvalidState         = errors.New("invalid session state")
	ErrIOError              = errors.New("I/O error")
	ErrTimeout              = errors.New("operation timeout")
	ErrBufferOverflow       = errors.New("buffer overflow")
	ErrProcessExited        = errors.New("process has exited")
	ErrPermission           = errors.New("permission denied")
	ErrNotInitialized       = errors.New("not initialized")
	ErrAlreadyInitialized   = errors.New("already initialized")
	ErrConnectionClosed     = errors.New("connection closed")
	ErrInvalidSeq           = errors.New("invalid sequence number")
	ErrRequestFailed        = errors.New("request failed")
	ErrNotFound             = errors.New("resource not found")
	ErrBufferTooSmall       = errors.New("buffer too small")
	ErrNotSupported         = errors.New("operation not supported")
)

// MDError represents an error from the Magic Debugger library
type MDError struct {
	Code int
	Msg  string
}

func (e *MDError) Error() string {
	return fmt.Sprintf("magic debugger error %d: %s", e.Code, e.Msg)
}

// cError converts C error code to Go error
func cError(code C.md_error_t) error {
	if code == C.MD_SUCCESS {
		return nil
	}
	switch code {
	case C.MD_ERROR_UNKNOWN:
		return ErrUnknown
	case C.MD_ERROR_INVALID_ARG:
		return ErrInvalidArg
	case C.MD_ERROR_NULL_POINTER:
		return ErrNullPointer
	case C.MD_ERROR_OUT_OF_MEMORY:
		return ErrOutOfMemory
	case C.MD_ERROR_PIPE_FAILED:
		return ErrPipeFailed
	case C.MD_ERROR_FORK_FAILED:
		return ErrForkFailed
	case C.MD_ERROR_EXEC_FAILED:
		return ErrExecFailed
	case C.MD_ERROR_SESSION_NOT_FOUND:
		return ErrSessionNotFound
	case C.MD_ERROR_SESSION_EXISTS:
		return ErrSessionExists
	case C.MD_ERROR_INVALID_STATE:
		return ErrInvalidState
	case C.MD_ERROR_IO_ERROR:
		return ErrIOError
	case C.MD_ERROR_TIMEOUT:
		return ErrTimeout
	case C.MD_ERROR_BUFFER_OVERFLOW:
		return ErrBufferOverflow
	case C.MD_ERROR_PROCESS_EXITED:
		return ErrProcessExited
	case C.MD_ERROR_PERMISSION:
		return ErrPermission
	case C.MD_ERROR_NOT_INITIALIZED:
		return ErrNotInitialized
	case C.MD_ERROR_ALREADY_INITIALIZED:
		return ErrAlreadyInitialized
	case C.MD_ERROR_CONNECTION_CLOSED:
		return ErrConnectionClosed
	case C.MD_ERROR_INVALID_SEQ:
		return ErrInvalidSeq
	case C.MD_ERROR_REQUEST_FAILED:
		return ErrRequestFailed
	case C.MD_ERROR_NOT_FOUND:
		return ErrNotFound
	case C.MD_ERROR_BUFFER_TOO_SMALL:
		return ErrBufferTooSmall
	case C.MD_ERROR_NOT_SUPPORTED:
		return ErrNotSupported
	default:
		return fmt.Errorf("unknown error code: %d", int(code))
	}
}

// Helper functions for string conversion
func cString(s string) *C.char {
	return C.CString(s)
}

func goString(s *C.char) string {
	if s == nil {
		return ""
	}
	return C.GoString(s)
}

func freeCString(s *C.char) {
	C.free(unsafe.Pointer(s))
}

// ExecState represents program execution state
type ExecState int

const (
	ExecStateNone       ExecState = iota
	ExecStateLaunching            // Program is being launched
	ExecStateRunning              // Program is running
	ExecStateStopped              // Program is stopped
	ExecStateStepping             // Program is stepping
	ExecStateExiting              // Program is exiting
	ExecStateTerminated           // Program has terminated
	ExecStateCrashed              // Program crashed
)

func (s ExecState) String() string {
	switch s {
	case ExecStateNone:
		return "none"
	case ExecStateLaunching:
		return "launching"
	case ExecStateRunning:
		return "running"
	case ExecStateStopped:
		return "stopped"
	case ExecStateStepping:
		return "stepping"
	case ExecStateExiting:
		return "exiting"
	case ExecStateTerminated:
		return "terminated"
	case ExecStateCrashed:
		return "crashed"
	default:
		return "unknown"
	}
}

func cExecState(s C.md_exec_state_t) ExecState {
	return ExecState(s)
}

// StopReason represents why the program stopped
type StopReason int

const (
	StopReasonUnknown StopReason = iota
	StopReasonStep
	StopReasonBreakpoint
	StopReasonException
	StopReasonPause
	StopReasonEntry
	StopReasonGoto
	StopReasonFunctionBreakpoint
	StopReasonDataBreakpoint
	StopReasonInstructionBreakpoint
	StopReasonSignal
	StopReasonWatchpoint
)

func (r StopReason) String() string {
	switch r {
	case StopReasonUnknown:
		return "unknown"
	case StopReasonStep:
		return "step"
	case StopReasonBreakpoint:
		return "breakpoint"
	case StopReasonException:
		return "exception"
	case StopReasonPause:
		return "pause"
	case StopReasonEntry:
		return "entry"
	case StopReasonGoto:
		return "goto"
	case StopReasonFunctionBreakpoint:
		return "function_breakpoint"
	case StopReasonDataBreakpoint:
		return "data_breakpoint"
	case StopReasonInstructionBreakpoint:
		return "instruction_breakpoint"
	case StopReasonSignal:
		return "signal"
	case StopReasonWatchpoint:
		return "watchpoint"
	default:
		return "unknown"
	}
}

func cStopReason(r C.md_stop_reason_t) StopReason {
	return StopReason(r)
}

// DebuggerType represents debugger backend type
type DebuggerType int

const (
	DebuggerNone   DebuggerType = iota
	DebuggerLLDB                // LLDB debugger
	DebuggerGDB                 // GDB debugger
	DebuggerShell               // Shell debugger (bashdb)
	DebuggerCustom              // Custom debugger
)

func (t DebuggerType) String() string {
	switch t {
	case DebuggerNone:
		return "none"
	case DebuggerLLDB:
		return "lldb"
	case DebuggerGDB:
		return "gdb"
	case DebuggerShell:
		return "shell"
	case DebuggerCustom:
		return "custom"
	default:
		return "unknown"
	}
}

func cDebuggerType(t C.md_debugger_type_t) DebuggerType {
	return DebuggerType(t)
}

func goDebuggerType(t DebuggerType) C.md_debugger_type_t {
	return C.md_debugger_type_t(t)
}

// ThreadState represents thread execution state
type ThreadState int

const (
	ThreadStateUnknown ThreadState = iota
	ThreadStateStopped
	ThreadStateRunning
	ThreadStateStepping
	ThreadStateExited
)

func (s ThreadState) String() string {
	switch s {
	case ThreadStateUnknown:
		return "unknown"
	case ThreadStateStopped:
		return "stopped"
	case ThreadStateRunning:
		return "running"
	case ThreadStateStepping:
		return "stepping"
	case ThreadStateExited:
		return "exited"
	default:
		return "unknown"
	}
}

// BreakpointState represents breakpoint state
type BreakpointState int

const (
	BPStateInvalid BreakpointState = iota
	BPStatePending
	BPStateVerified
	BPStateFailed
	BPStateRemoved
	BPStateDisabled
)

func (s BreakpointState) String() string {
	switch s {
	case BPStateInvalid:
		return "invalid"
	case BPStatePending:
		return "pending"
	case BPStateVerified:
		return "verified"
	case BPStateFailed:
		return "failed"
	case BPStateRemoved:
		return "removed"
	case BPStateDisabled:
		return "disabled"
	default:
		return "unknown"
	}
}

// BreakpointType represents breakpoint type
type BreakpointType int

const (
	BPTypeLine BreakpointType = iota
	BPTypeFunction
	BPTypeConditional
	BPTypeData
	BPTypeInstruction
	BPTypeLogpoint
)

func (t BreakpointType) String() string {
	switch t {
	case BPTypeLine:
		return "line"
	case BPTypeFunction:
		return "function"
	case BPTypeConditional:
		return "conditional"
	case BPTypeData:
		return "data"
	case BPTypeInstruction:
		return "instruction"
	case BPTypeLogpoint:
		return "logpoint"
	default:
		return "unknown"
	}
}

// StepGranularity represents step granularity
type StepGranularity int

const (
	StepLine StepGranularity = iota
	StepStatement
	StepInstruction
)

func goStepGranularity(g StepGranularity) C.md_step_granularity_t {
	return C.md_step_granularity_t(g)
}

// Set finalizer for proper cleanup
func setFinalizer(obj interface{}, cleanup func()) {
	runtime.SetFinalizer(obj, func(_ interface{}) {
		cleanup()
	})
}
