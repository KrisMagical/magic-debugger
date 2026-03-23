/**
 * @file dap_protocol.h
 * @brief DAP Protocol Constants and Helpers
 * 
 * This file contains protocol constants, command names, event names,
 * and helper functions for the Debug Adapter Protocol.
 */

#ifndef MAGIC_DEBUGGER_DAP_PROTOCOL_H
#define MAGIC_DEBUGGER_DAP_PROTOCOL_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Protocol Version
 * ============================================================================ */

/** DAP protocol version string */
#define DAP_PROTOCOL_VERSION_STRING "1.65.0"

/* ============================================================================
 * Content Type
 * ============================================================================ */

/** Default content type for DAP messages */
#define DAP_CONTENT_TYPE "application/vscode-jsonrpc; charset=utf-8"

/** Content-Length header prefix */
#define DAP_HEADER_CONTENT_LENGTH "Content-Length"

/** Content-Type header prefix */
#define DAP_HEADER_CONTENT_TYPE "Content-Type"

/** Header terminator */
#define DAP_HEADER_TERMINATOR "\r\n\r\n"

/** Header line terminator */
#define DAP_HEADER_LINE_TERMINATOR "\r\n"

/* ============================================================================
 * Request Commands
 * ============================================================================ */

/* Lifecycle Requests */
#define DAP_CMD_INITIALIZE         "initialize"
#define DAP_CMD_LAUNCH             "launch"
#define DAP_CMD_ATTACH             "attach"
#define DAP_CMD_DISCONNECT         "disconnect"
#define DAP_CMD_TERMINATE          "terminate"
#define DAP_CMD_CONFIGURATION_DONE "configurationDone"

/* Execution Control Requests */
#define DAP_CMD_CONTINUE           "continue"
#define DAP_CMD_NEXT               "next"
#define DAP_CMD_STEP_IN            "stepIn"
#define DAP_CMD_STEP_OUT           "stepOut"
#define DAP_CMD_STEP_BACK          "stepBack"
#define DAP_CMD_REVERSE_CONTINUE   "reverseContinue"
#define DAP_CMD_RESTART_FRAME      "restartFrame"
#define DAP_CMD_GOTO               "goto"
#define DAP_CMD_PAUSE              "pause"

/* Breakpoint Requests */
#define DAP_CMD_SET_BREAKPOINTS            "setBreakpoints"
#define DAP_CMD_SET_FUNCTION_BREAKPOINTS   "setFunctionBreakpoints"
#define DAP_CMD_SET_EXCEPTION_BREAKPOINTS  "setExceptionBreakpoints"
#define DAP_CMD_SET_INSTRUCTION_BREAKPOINTS "setInstructionBreakpoints"
#define DAP_CMD_SET_DATA_BREAKPOINTS       "setDataBreakpoints"
#define DAP_CMD_BREAKPOINT_LOCATIONS       "breakpointLocations"

/* Stack/Thread Requests */
#define DAP_CMD_THREADS            "threads"
#define DAP_CMD_STACK_TRACE        "stackTrace"
#define DAP_CMD_SCOPES             "scopes"
#define DAP_CMD_VARIABLES          "variables"
#define DAP_CMD_SET_VARIABLE       "setVariable"

/* Evaluation Requests */
#define DAP_CMD_EVALUATE           "evaluate"
#define DAP_CMD_SET_EXPRESSION     "setExpression"
#define DAP_CMD_COMPLETIONS        "completions"

/* Memory Requests */
#define DAP_CMD_READ_MEMORY        "readMemory"
#define DAP_CMD_DISASSEMBLE        "disassemble"

/* Module Requests */
#define DAP_CMD_MODULES            "modules"
#define DAP_CMD_LOADED_SOURCES     "loadedSources"

/* Source Requests */
#define DAP_CMD_SOURCE             "source"

/* Exception Requests */
#define DAP_CMD_EXCEPTION_INFO     "exceptionInfo"

/* Other Requests */
#define DAP_CMD_RUN_IN_TERMINAL    "runInTerminal"
#define DAP_CMD_CANCEL             "cancel"

/* ============================================================================
 * Event Names
 * ============================================================================ */

/* Initialization Events */
#define DAP_EVENT_INITIALIZED      "initialized"

/* Execution Events */
#define DAP_EVENT_STOPPED          "stopped"
#define DAP_EVENT_CONTINUED        "continued"
#define DAP_EVENT_EXITED           "exited"
#define DAP_EVENT_TERMINATED       "terminated"

/* Thread Events */
#define DAP_EVENT_THREAD           "thread"
#define DAP_EVENT_THREAD_STARTED   "threadStarted"   /* Deprecated */
#define DAP_EVENT_THREAD_EXITED    "threadExited"    /* Deprecated */

/* Module Events */
#define DAP_EVENT_MODULE           "module"

/* Output Events */
#define DAP_EVENT_OUTPUT           "output"

/* Breakpoint Events */
#define DAP_EVENT_BREAKPOINT       "breakpoint"

/* Capability Events */
#define DAP_EVENT_CAPABILITIES     "capabilities"

/* Progress Events */
#define DAP_EVENT_PROGRESS_START   "progressStart"
#define DAP_EVENT_PROGRESS_UPDATE  "progressUpdate"
#define DAP_EVENT_PROGRESS_END     "progressEnd"

/* Invalidated Event */
#define DAP_EVENT_INVALIDATED      "invalidated"

/* Memory Event */
#define DAP_EVENT_MEMORY           "memory"

/* ============================================================================
 * Stop Reasons
 * ============================================================================ */

#define DAP_STOP_REASON_STEP               "step"
#define DAP_STOP_REASON_BREAKPOINT         "breakpoint"
#define DAP_STOP_REASON_EXCEPTION          "exception"
#define DAP_STOP_REASON_PAUSE              "pause"
#define DAP_STOP_REASON_ENTRY              "entry"
#define DAP_STOP_REASON_GOTO               "goto"
#define DAP_STOP_REASON_FUNCTION_BREAKPOINT "function breakpoint"
#define DAP_STOP_REASON_DATA_BREAKPOINT    "data breakpoint"
#define DAP_STOP_REASON_INSTRUCTION_BREAKPOINT "instruction breakpoint"

/* ============================================================================
 * Thread Reasons
 * ============================================================================ */

#define DAP_THREAD_REASON_STARTED  "started"
#define DAP_THREAD_REASON_EXITED   "exited"

/* ============================================================================
 * Module Reasons
 * ============================================================================ */

#define DAP_MODULE_REASON_NEW      "new"
#define DAP_MODULE_REASON_CHANGED  "changed"
#define DAP_MODULE_REASON_REMOVED  "removed"

/* ============================================================================
 * Output Categories
 * ============================================================================ */

#define DAP_OUTPUT_CATEGORY_CONSOLE    "console"
#define DAP_OUTPUT_CATEGORY_STDOUT     "stdout"
#define DAP_OUTPUT_CATEGORY_STDERR     "stderr"
#define DAP_OUTPUT_CATEGORY_TELEMETRY  "telemetry"

/* ============================================================================
 * Path Formats
 * ============================================================================ */

#define DAP_PATH_FORMAT_PATH    "path"
#define DAP_PATH_FORMAT_URI     "uri"

/* ============================================================================
 * Stepping Granularity
 * ============================================================================ */

#define DAP_GRANULARITY_STATEMENT   "statement"
#define DAP_GRANULARITY_LINE        "line"
#define DAP_GRANULARITY_INSTRUCTION "instruction"

/* ============================================================================
 * JSON Field Names
 * ============================================================================ */

/* Message Fields */
#define DAP_FIELD_SEQ              "seq"
#define DAP_FIELD_TYPE             "type"
#define DAP_FIELD_COMMAND          "command"
#define DAP_FIELD_REQUEST_SEQ      "request_seq"
#define DAP_FIELD_SUCCESS          "success"
#define DAP_FIELD_MESSAGE          "message"
#define DAP_FIELD_BODY             "body"
#define DAP_FIELD_EVENT            "event"
#define DAP_FIELD_ARGUMENTS        "arguments"

/* Initialize Fields */
#define DAP_FIELD_CLIENT_ID        "clientID"
#define DAP_FIELD_CLIENT_NAME      "clientName"
#define DAP_FIELD_ADAPTER_ID       "adapterID"
#define DAP_FIELD_LOCALE           "locale"
#define DAP_FIELD_LINES_START_AT1  "linesStartAt1"
#define DAP_FIELD_PATH_FORMAT      "pathFormat"
#define DAP_FIELD_CAPABILITIES     "capabilities"

/* Launch/Attach Fields */
#define DAP_FIELD_NO_DEBUG         "noDebug"
#define DAP_FIELD_STOP_ON_ENTRY    "stopOnEntry"
#define DAP_FIELD_PROGRAM          "program"
#define DAP_FIELD_ARGS             "args"
#define DAP_FIELD_CWD              "cwd"
#define DAP_FIELD_ENV              "env"
#define DAP_FIELD_PROCESS_ID       "processId"

/* Thread Fields */
#define DAP_FIELD_THREAD_ID        "threadId"
#define DAP_FIELD_THREAD_NAME      "name"
#define DAP_FIELD_THREADS          "threads"
#define DAP_FIELD_REASON           "reason"

/* Stack Trace Fields */
#define DAP_FIELD_STACK_FRAMES     "stackFrames"
#define DAP_FIELD_TOTAL_FRAMES     "totalFrames"
#define DAP_FIELD_START_FRAME      "startFrame"
#define DAP_FIELD_LEVELS           "levels"

/* Frame Fields */
#define DAP_FIELD_ID               "id"
#define DAP_FIELD_NAME             "name"
#define DAP_FIELD_SOURCE           "source"
#define DAP_FIELD_LINE             "line"
#define DAP_FIELD_COLUMN           "column"
#define DAP_FIELD_END_LINE         "endLine"
#define DAP_FIELD_END_COLUMN       "endColumn"

/* Source Fields */
#define DAP_FIELD_PATH             "path"
#define DAP_FIELD_SOURCE_REFERENCE "sourceReference"
#define DAP_FIELD_PRESENTATION_HINT "presentationHint"
#define DAP_FIELD_ORIGIN           "origin"
#define DAP_FIELD_SOURCES          "sources"

/* Breakpoint Fields */
#define DAP_FIELD_BREAKPOINTS      "breakpoints"
#define DAP_FIELD_BREAKPOINT_ID    "id"
#define DAP_FIELD_VERIFIED         "verified"
#define DAP_FIELD_CONDITION        "condition"
#define DAP_FIELD_HIT_CONDITION    "hitCondition"
#define DAP_FIELD_LOG_MESSAGE      "logMessage"

/* Variable Fields */
#define DAP_FIELD_VARIABLES         "variables"
#define DAP_FIELD_VARIABLES_REFERENCE "variablesReference"
#define DAP_FIELD_NAMED_VARIABLES   "namedVariables"
#define DAP_FIELD_INDEXED_VARIABLES "indexedVariables"
#define DAP_FIELD_VALUE            "value"
#define DAP_FIELD_TYPE             "type"
#define DAP_FIELD_EVALUATE_NAME    "evaluateName"
#define DAP_FIELD_MEMORY_REFERENCE "memoryReference"

/* Scope Fields */
#define DAP_FIELD_SCOPES           "scopes"
#define DAP_FIELD_EXPENSIVE        "expensive"

/* Evaluate Fields */
#define DAP_FIELD_EXPRESSION       "expression"
#define DAP_FIELD_CONTEXT          "context"
#define DAP_FIELD_FRAME_ID         "frameId"
#define DAP_FIELD_RESULT           "result"

/* Output Fields */
#define DAP_FIELD_OUTPUT           "output"
#define DAP_FIELD_CATEGORY         "category"
#define DAP_FIELD_GROUP            "group"
#define DAP_FIELD_VARIABLES_REFERENCE "variablesReference"
#define DAP_FIELD_SOURCE           "source"

/* Exit Fields */
#define DAP_FIELD_EXIT_CODE        "exitCode"

/* Continue Fields */
#define DAP_FIELD_ALL_THREADS_CONTINUED "allThreadsContinued"
#define DAP_FIELD_SINGLE_THREAD    "singleThread"

/* ============================================================================
 * Error Codes (DAP Protocol)
 * ============================================================================ */

/** Request cancelled */
#define DAP_ERROR_CODE_CANCELLED           1

/** Configuration error */
#define DAP_ERROR_CODE_CONFIGURATION       2

/** Not supported */
#define DAP_ERROR_CODE_NOT_SUPPORTED       3

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Check if a command is a valid DAP command
 * @param command Command name
 * @return true if valid
 */
MD_API bool dap_is_valid_command(const char *command);

/**
 * @brief Check if an event is a valid DAP event
 * @param event Event name
 * @return true if valid
 */
MD_API bool dap_is_valid_event(const char *event);

/**
 * @brief Get the category name for output
 * @param category Category string
 * @return Human-readable category name
 */
MD_API const char* dap_output_category_name(const char *category);

/**
 * @brief Check if a stop reason requires user attention
 * @param reason Stop reason string
 * @return true if requires attention
 */
MD_API bool dap_stop_reason_requires_attention(const char *reason);

#ifdef __cplusplus
}
#endif

#endif /* MAGIC_DEBUGGER_DAP_PROTOCOL_H */
