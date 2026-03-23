// Package server provides WebSocket and HTTP API servers for AI integration.
// This package implements real-time communication between AI systems and the debugger.
package server

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"sync"
	"time"

	"github.com/gorilla/websocket"
	"github.com/magic-debugger/ai-interface/api"
)

// ============================================================================
// WebSocket Configuration
// ============================================================================

// WSConfig represents WebSocket server configuration
type WSConfig struct {
	// Server settings
	Host            string        `json:"host"`
	Port            int           `json:"port"`
	Path            string        `json:"path"`
	ReadBufferSize  int           `json:"read_buffer_size"`
	WriteBufferSize int           `json:"write_buffer_size"`

	// Timeouts
	WriteWait      time.Duration `json:"write_wait"`
	PongWait       time.Duration `json:"pong_wait"`
	PingPeriod     time.Duration `json:"ping_period"`
	MaxMessageSize int64         `json:"max_message_size"`

	// Security
	AllowedOrigins []string `json:"allowed_origins"`
	AuthToken      string   `json:"auth_token"`
}

// DefaultWSConfig returns default WebSocket configuration
func DefaultWSConfig() WSConfig {
	return WSConfig{
		Host:            "0.0.0.0",
		Port:            8765,
		Path:            "/ws",
		ReadBufferSize:  4096,
		WriteBufferSize: 4096,
		WriteWait:       10 * time.Second,
		PongWait:        60 * time.Second,
		PingPeriod:      54 * time.Second,
		MaxMessageSize:  65536,
		AllowedOrigins:  []string{"*"},
	}
}

// ============================================================================
// WebSocket Message Types
// ============================================================================

// MessageType represents WebSocket message type
type MessageType string

const (
	// Request types from AI
	MsgTypeInitialize     MessageType = "initialize"
	MsgTypeLaunch         MessageType = "launch"
	MsgTypeAttach         MessageType = "attach"
	MsgTypeDisconnect     MessageType = "disconnect"
	MsgTypeContinue       MessageType = "continue"
	MsgTypePause          MessageType = "pause"
	MsgTypeStepOver       MessageType = "step_over"
	MsgTypeStepInto       MessageType = "step_into"
	MsgTypeStepOut        MessageType = "step_out"
	MsgTypeSetBreakpoint  MessageType = "set_breakpoint"
	MsgTypeRemoveBP       MessageType = "remove_breakpoint"
	MsgTypeGetState       MessageType = "get_state"
	MsgTypeGetVariables   MessageType = "get_variables"
	MsgTypeEvaluate       MessageType = "evaluate"
	MsgTypeGetStackTrace  MessageType = "get_stack_trace"
	MsgTypeGetThreads     MessageType = "get_threads"
	MsgTypeGetSource      MessageType = "get_source"
	MsgTypeSetActiveThread MessageType = "set_active_thread"
	MsgTypeSetActiveFrame  MessageType = "set_active_frame"
	MsgTypeRestart        MessageType = "restart"
	MsgTypeGetBreakpoints MessageType = "get_breakpoints"
	MsgTypeWaitForStop    MessageType = "wait_for_stop"

	// Response and event types to AI
	MsgTypeResponse      MessageType = "response"
	MsgTypeEvent         MessageType = "event"
	MsgTypeStateChanged  MessageType = "state_changed"
	MsgTypeBreakpointHit MessageType = "breakpoint_hit"
	MsgTypeStepComplete  MessageType = "step_complete"
	MsgTypeException     MessageType = "exception"
	MsgTypeProcessExited MessageType = "process_exited"
	MsgTypeOutput        MessageType = "output"
	MsgTypeError         MessageType = "error"
)

// WSMessage represents a WebSocket message
type WSMessage struct {
	ID        int64       `json:"id"`
	Type      MessageType `json:"type"`
	Timestamp time.Time   `json:"timestamp"`
	Data      interface{} `json:"data,omitempty"`
}

// WSResponse represents a WebSocket response
type WSResponse struct {
	ID        int64       `json:"id"`
	Type      MessageType `json:"type"`
	Timestamp time.Time   `json:"timestamp"`
	Success   bool        `json:"success"`
	Data      interface{} `json:"data,omitempty"`
	Error     string      `json:"error,omitempty"`
}

// WSEvent represents a WebSocket event
type WSEvent struct {
	Type      MessageType `json:"type"`
	Timestamp time.Time   `json:"timestamp"`
	Data      interface{} `json:"data,omitempty"`
}

// ============================================================================
// Client Connection
// ============================================================================

// WSClient represents a WebSocket client connection
type WSClient struct {
	conn   *websocket.Conn
	server *WSServer
	sendCh chan []byte
	doneCh chan struct{}
	mu     sync.Mutex
	ctx    context.Context
	cancel context.CancelFunc
}

// NewWSClient creates a new WebSocket client
func NewWSClient(conn *websocket.Conn, server *WSServer) *WSClient {
	ctx, cancel := context.WithCancel(context.Background())
	return &WSClient{
		conn:   conn,
		server: server,
		sendCh: make(chan []byte, 256),
		doneCh: make(chan struct{}),
		ctx:    ctx,
		cancel: cancel,
	}
}

// Send sends a message to the client
func (c *WSClient) Send(msg interface{}) error {
	data, err := json.Marshal(msg)
	if err != nil {
		return err
	}

	select {
	case c.sendCh <- data:
		return nil
	case <-c.doneCh:
		return fmt.Errorf("client disconnected")
	default:
		return fmt.Errorf("send buffer full")
	}
}

// Close closes the client connection
func (c *WSClient) Close() {
	c.cancel()
	close(c.doneCh)
	c.conn.Close()
}

// readPump handles incoming messages from the client
func (c *WSClient) readPump() {
	defer func() {
		c.server.unregister <- c
		c.Close()
	}()

	config := c.server.config
	c.conn.SetReadLimit(config.MaxMessageSize)
	c.conn.SetReadDeadline(time.Now().Add(config.PongWait))
	c.conn.SetPongHandler(func(string) error {
		c.conn.SetReadDeadline(time.Now().Add(config.PongWait))
		return nil
	})

	for {
		_, message, err := c.conn.ReadMessage()
		if err != nil {
			break
		}

		c.handleMessage(message)
	}
}

// writePump handles outgoing messages to the client
func (c *WSClient) writePump() {
	config := c.server.config
	ticker := time.NewTicker(config.PingPeriod)
	defer func() {
		ticker.Stop()
		c.Close()
	}()

	for {
		select {
		case <-c.doneCh:
			return
		case message, ok := <-c.sendCh:
			c.conn.SetWriteDeadline(time.Now().Add(config.WriteWait))
			if !ok {
				c.conn.WriteMessage(websocket.CloseMessage, []byte{})
				return
			}

			w, err := c.conn.NextWriter(websocket.TextMessage)
			if err != nil {
				return
			}
			w.Write(message)

			// Drain the channel
			n := len(c.sendCh)
			for i := 0; i < n; i++ {
				w.Write([]byte{'\n'})
				w.Write(<-c.sendCh)
			}

			if err := w.Close(); err != nil {
				return
			}

		case <-ticker.C:
			c.conn.SetWriteDeadline(time.Now().Add(config.WriteWait))
			if err := c.conn.WriteMessage(websocket.PingMessage, nil); err != nil {
				return
			}
		}
	}
}

// handleMessage processes incoming messages
func (c *WSClient) handleMessage(data []byte) {
	var msg WSMessage
	if err := json.Unmarshal(data, &msg); err != nil {
		c.sendError(0, "invalid message format: "+err.Error())
		return
	}

	msg.Timestamp = time.Now()

	var response *WSResponse
	switch msg.Type {
	case MsgTypeInitialize:
		response = c.handleInitialize(&msg)
	case MsgTypeLaunch:
		response = c.handleLaunch(&msg)
	case MsgTypeAttach:
		response = c.handleAttach(&msg)
	case MsgTypeDisconnect:
		response = c.handleDisconnect(&msg)
	case MsgTypeContinue:
		response = c.handleContinue(&msg)
	case MsgTypePause:
		response = c.handlePause(&msg)
	case MsgTypeStepOver:
		response = c.handleStepOver(&msg)
	case MsgTypeStepInto:
		response = c.handleStepInto(&msg)
	case MsgTypeStepOut:
		response = c.handleStepOut(&msg)
	case MsgTypeSetBreakpoint:
		response = c.handleSetBreakpoint(&msg)
	case MsgTypeRemoveBP:
		response = c.handleRemoveBreakpoint(&msg)
	case MsgTypeGetState:
		response = c.handleGetState(&msg)
	case MsgTypeGetVariables:
		response = c.handleGetVariables(&msg)
	case MsgTypeEvaluate:
		response = c.handleEvaluate(&msg)
	case MsgTypeGetStackTrace:
		response = c.handleGetStackTrace(&msg)
	case MsgTypeGetThreads:
		response = c.handleGetThreads(&msg)
	case MsgTypeGetSource:
		response = c.handleGetSource(&msg)
	case MsgTypeSetActiveThread:
		response = c.handleSetActiveThread(&msg)
	case MsgTypeSetActiveFrame:
		response = c.handleSetActiveFrame(&msg)
	case MsgTypeRestart:
		response = c.handleRestart(&msg)
	case MsgTypeGetBreakpoints:
		response = c.handleGetBreakpoints(&msg)
	case MsgTypeWaitForStop:
		response = c.handleWaitForStop(&msg)
	default:
		response = &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "unknown message type: " + string(msg.Type),
		}
	}

	if response != nil {
		c.Send(response)
	}
}

// sendError sends an error response
func (c *WSClient) sendError(id int64, errMsg string) {
	c.Send(&WSResponse{
		ID:        id,
		Type:      MsgTypeError,
		Timestamp: time.Now(),
		Success:   false,
		Error:     errMsg,
	})
}

// ============================================================================
// Message Handlers
// ============================================================================

func (c *WSClient) handleInitialize(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not configured",
		}
	}

	if err := debugger.Initialize(c.ctx); err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
		Data: map[string]interface{}{
			"adapter_name":    debugger.GetAdapterName(),
			"adapter_version": debugger.GetAdapterVersion(),
		},
	}
}

func (c *WSClient) handleLaunch(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not configured",
		}
	}

	if err := debugger.Launch(c.ctx); err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	state := debugger.GetState()
	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
		Data:      state,
	}
}

func (c *WSClient) handleAttach(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not configured",
		}
	}

	var data struct {
		PID int `json:"pid"`
	}
	if msg.Data != nil {
		jsonData, _ := json.Marshal(msg.Data)
		json.Unmarshal(jsonData, &data)
	}

	if data.PID <= 0 {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "invalid PID",
		}
	}

	if err := debugger.Attach(c.ctx, data.PID); err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	state := debugger.GetState()
	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
		Data:      state,
	}
}

func (c *WSClient) handleDisconnect(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   true,
		}
	}

	if err := debugger.Close(); err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
	}
}

func (c *WSClient) handleContinue(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil || !debugger.IsRunning() {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not running",
		}
	}

	if err := debugger.Continue(c.ctx); err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
	}
}

func (c *WSClient) handlePause(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil || !debugger.IsRunning() {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not running",
		}
	}

	if err := debugger.Pause(c.ctx); err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	state := debugger.GetState()
	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
		Data:      state,
	}
}

func (c *WSClient) handleStepOver(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil || !debugger.IsRunning() {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not running",
		}
	}

	if err := debugger.StepOver(c.ctx); err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
	}
}

func (c *WSClient) handleStepInto(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil || !debugger.IsRunning() {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not running",
		}
	}

	if err := debugger.StepInto(c.ctx); err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
	}
}

func (c *WSClient) handleStepOut(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil || !debugger.IsRunning() {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not running",
		}
	}

	if err := debugger.StepOut(c.ctx); err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
	}
}

func (c *WSClient) handleSetBreakpoint(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not configured",
		}
	}

	var data struct {
		File      string `json:"file"`
		Line      int    `json:"line"`
		Condition string `json:"condition"`
	}
	if msg.Data != nil {
		jsonData, _ := json.Marshal(msg.Data)
		json.Unmarshal(jsonData, &data)
	}

	bp, err := debugger.SetBreakpoint(data.File, data.Line, data.Condition)
	if err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
		Data:      bp,
	}
}

func (c *WSClient) handleRemoveBreakpoint(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not configured",
		}
	}

	var data struct {
		ID int `json:"id"`
	}
	if msg.Data != nil {
		jsonData, _ := json.Marshal(msg.Data)
		json.Unmarshal(jsonData, &data)
	}

	if err := debugger.RemoveBreakpoint(data.ID); err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
	}
}

func (c *WSClient) handleGetState(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not configured",
		}
	}

	state := debugger.GetState()
	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
		Data:      state,
	}
}

func (c *WSClient) handleGetVariables(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not configured",
		}
	}

	var data struct {
		FrameID   int    `json:"frame_id"`
		ScopeType string `json:"scope_type"`
	}
	if msg.Data != nil {
		jsonData, _ := json.Marshal(msg.Data)
		json.Unmarshal(jsonData, &data)
	}

	vars, err := debugger.GetVariables(data.FrameID, data.ScopeType)
	if err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
		Data:      vars,
	}
}

func (c *WSClient) handleEvaluate(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not configured",
		}
	}

	var data struct {
		Expression string `json:"expression"`
		FrameID    int    `json:"frame_id"`
	}
	if msg.Data != nil {
		jsonData, _ := json.Marshal(msg.Data)
		json.Unmarshal(jsonData, &data)
	}

	result, err := debugger.Evaluate(data.Expression, data.FrameID)
	if err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
		Data:      result,
	}
}

func (c *WSClient) handleGetStackTrace(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not configured",
		}
	}

	var data struct {
		ThreadID int `json:"thread_id"`
	}
	if msg.Data != nil {
		jsonData, _ := json.Marshal(msg.Data)
		json.Unmarshal(jsonData, &data)
	}

	frames, err := debugger.GetStackTrace(data.ThreadID)
	if err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
		Data:      frames,
	}
}

func (c *WSClient) handleGetThreads(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not configured",
		}
	}

	threads, err := debugger.GetThreads()
	if err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
		Data:      threads,
	}
}

func (c *WSClient) handleGetSource(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not configured",
		}
	}

	var data struct {
		File      string `json:"file"`
		StartLine int    `json:"start_line"`
		EndLine   int    `json:"end_line"`
	}
	if msg.Data != nil {
		jsonData, _ := json.Marshal(msg.Data)
		json.Unmarshal(jsonData, &data)
	}

	lines, err := debugger.GetSource(data.File, data.StartLine, data.EndLine)
	if err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
		Data:      lines,
	}
}

func (c *WSClient) handleSetActiveThread(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not configured",
		}
	}

	var data struct {
		ThreadID int `json:"thread_id"`
	}
	if msg.Data != nil {
		jsonData, _ := json.Marshal(msg.Data)
		json.Unmarshal(jsonData, &data)
	}

	if err := debugger.SetActiveThread(data.ThreadID); err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
	}
}

func (c *WSClient) handleSetActiveFrame(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not configured",
		}
	}

	var data struct {
		FrameID int `json:"frame_id"`
	}
	if msg.Data != nil {
		jsonData, _ := json.Marshal(msg.Data)
		json.Unmarshal(jsonData, &data)
	}

	if err := debugger.SetActiveFrame(data.FrameID); err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
	}
}

func (c *WSClient) handleRestart(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil || !debugger.IsRunning() {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not running",
		}
	}

	if err := debugger.Restart(c.ctx); err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
	}
}

func (c *WSClient) handleGetBreakpoints(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not configured",
		}
	}

	bps, err := debugger.GetBreakpoints()
	if err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
		Data:      bps,
	}
}

func (c *WSClient) handleWaitForStop(msg *WSMessage) *WSResponse {
	debugger := c.server.debugger
	if debugger == nil || !debugger.IsRunning() {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     "debugger not running",
		}
	}

	var data struct {
		TimeoutMs int `json:"timeout_ms"`
	}
	if msg.Data != nil {
		jsonData, _ := json.Marshal(msg.Data)
		json.Unmarshal(jsonData, &data)
	}

	timeout := time.Duration(data.TimeoutMs) * time.Millisecond
	if timeout <= 0 {
		timeout = 30 * time.Second
	}

	if err := debugger.WaitForStop(c.ctx, timeout); err != nil {
		return &WSResponse{
			ID:        msg.ID,
			Type:      MsgTypeResponse,
			Timestamp: time.Now(),
			Success:   false,
			Error:     err.Error(),
		}
	}

	state := debugger.GetState()
	return &WSResponse{
		ID:        msg.ID,
		Type:      MsgTypeResponse,
		Timestamp: time.Now(),
		Success:   true,
		Data:      state,
	}
}

// ============================================================================
// WebSocket Server
// ============================================================================

// WSServer represents the WebSocket server
type WSServer struct {
	config     WSConfig
	debugger   *api.Debugger
	upgrader   websocket.Upgrader
	clients    map[*WSClient]bool
	register   chan *WSClient
	unregister chan *WSClient
	mu         sync.RWMutex
	ctx        context.Context
	cancel     context.CancelFunc
	httpServer *http.Server
}

// NewWSServer creates a new WebSocket server
func NewWSServer(config WSConfig, debugger *api.Debugger) *WSServer {
	ctx, cancel := context.WithCancel(context.Background())

	return &WSServer{
		config:   config,
		debugger: debugger,
		upgrader: websocket.Upgrader{
			ReadBufferSize:  config.ReadBufferSize,
			WriteBufferSize: config.WriteBufferSize,
			CheckOrigin: func(r *http.Request) bool {
				// Check allowed origins
				for _, origin := range config.AllowedOrigins {
					if origin == "*" {
						return true
					}
					if r.Header.Get("Origin") == origin {
						return true
					}
				}
				return false
			},
		},
		clients:    make(map[*WSClient]bool),
		register:   make(chan *WSClient),
		unregister: make(chan *WSClient),
		ctx:        ctx,
		cancel:     cancel,
	}
}

// Start starts the WebSocket server
func (s *WSServer) Start() error {
	mux := http.NewServeMux()
	mux.HandleFunc(s.config.Path, s.handleWebSocket)
	mux.HandleFunc("/health", s.handleHealth)

	addr := fmt.Sprintf("%s:%d", s.config.Host, s.config.Port)
	s.httpServer = &http.Server{
		Addr:    addr,
		Handler: mux,
	}

	// Start event broadcaster
	go s.broadcastEvents()

	// Start client manager
	go s.run()

	log.Printf("WebSocket server starting on %s%s", addr, s.config.Path)
	return s.httpServer.ListenAndServe()
}

// Shutdown shuts down the server
func (s *WSServer) Shutdown(ctx context.Context) error {
	s.cancel()

	// Close all clients
	s.mu.Lock()
	for client := range s.clients {
		client.Close()
	}
	s.clients = make(map[*WSClient]bool)
	s.mu.Unlock()

	if s.httpServer != nil {
		return s.httpServer.Shutdown(ctx)
	}
	return nil
}

// handleWebSocket handles WebSocket upgrade requests
func (s *WSServer) handleWebSocket(w http.ResponseWriter, r *http.Request) {
	conn, err := s.upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Printf("WebSocket upgrade error: %v", err)
		return
	}

	client := NewWSClient(conn, s)
	s.register <- client

	// Start read/write pumps
	go client.writePump()
	go client.readPump()
}

// handleHealth handles health check requests
func (s *WSServer) handleHealth(w http.ResponseWriter, r *http.Request) {
	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(map[string]interface{}{
		"status":    "healthy",
		"timestamp": time.Now(),
	})
}

// run manages client connections
func (s *WSServer) run() {
	for {
		select {
		case <-s.ctx.Done():
			return
		case client := <-s.register:
			s.mu.Lock()
			s.clients[client] = true
			s.mu.Unlock()
			log.Printf("Client connected. Total clients: %d", len(s.clients))

		case client := <-s.unregister:
			s.mu.Lock()
			if _, ok := s.clients[client]; ok {
				delete(s.clients, client)
				client.Close()
			}
			s.mu.Unlock()
			log.Printf("Client disconnected. Total clients: %d", len(s.clients))
		}
	}
}

// broadcastEvents broadcasts debugger events to all clients
func (s *WSServer) broadcastEvents() {
	if s.debugger == nil {
		return
	}

	events := s.debugger.Events()
	for {
		select {
		case <-s.ctx.Done():
			return
		case event, ok := <-events:
			if !ok {
				return
			}

			// Create WebSocket event
			wsEvent := WSEvent{
				Type:      MessageType(event.Type),
				Timestamp: event.Timestamp,
				Data:      event.Data,
			}

			// Broadcast to all clients
			s.mu.RLock()
			for client := range s.clients {
				go client.Send(wsEvent)
			}
			s.mu.RUnlock()
		}
	}
}

// Broadcast sends a message to all connected clients
func (s *WSServer) Broadcast(msgType MessageType, data interface{}) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	event := WSEvent{
		Type:      msgType,
		Timestamp: time.Now(),
		Data:      data,
	}

	for client := range s.clients {
		go client.Send(event)
	}
}

// GetClientCount returns the number of connected clients
func (s *WSServer) GetClientCount() int {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return len(s.clients)
}

// SetDebugger sets the debugger instance
func (s *WSServer) SetDebugger(debugger *api.Debugger) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.debugger = debugger
}
