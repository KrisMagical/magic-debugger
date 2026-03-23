// Package main provides the AI debug server CLI tool.
// This tool starts a WebSocket server that allows AI systems
// to interact with the Magic Debugger.
package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/magic-debugger/ai-interface/api"
	"github.com/magic-debugger/ai-interface/server"
)

// Build information (set by ldflags)
var (
	Version   = "0.1.0"
	BuildTime = "unknown"
	GitCommit = "unknown"
)

func main() {
	// Parse command line flags
	config := parseFlags()

	// Handle version
	if config.ShowVersion {
		fmt.Printf("Magic Debugger AI Interface v%s\n", Version)
		fmt.Printf("Build Time: %s\n", BuildTime)
		fmt.Printf("Git Commit: %s\n", GitCommit)
		os.Exit(0)
	}

	// Setup logging
	if config.Quiet {
		log.SetOutput(os.Stderr)
	} else {
		log.SetOutput(os.Stdout)
	}

	log.Printf("Magic Debugger AI Interface v%s starting...", Version)

	// Create debugger configuration
	debugConfig := api.DefaultConfig()
	debugConfig.DebuggerType = api.DebuggerType(config.DebuggerType)
	debugConfig.ProgramPath = config.ProgramPath
	debugConfig.ProgramArgs = config.ProgramArgs
	debugConfig.WorkingDir = config.WorkingDir
	debugConfig.StopOnEntry = config.StopOnEntry
	debugConfig.Timeout = time.Duration(config.TimeoutMs) * time.Millisecond

	// Create debugger instance
	debugger, err := api.NewDebugger(debugConfig)
	if err != nil {
		log.Fatalf("Failed to create debugger: %v", err)
	}
	defer debugger.Close()

	// Initialize debugger if program path is provided
	if config.ProgramPath != "" && config.AutoInit {
		ctx, cancel := context.WithTimeout(context.Background(), time.Duration(config.TimeoutMs)*time.Millisecond)
		if err := debugger.Initialize(ctx); err != nil {
			log.Printf("Warning: Failed to initialize debugger: %v", err)
		} else {
			log.Printf("Debugger initialized: %s", debugger.GetAdapterName())
		}
		cancel()
	}

	// Create WebSocket server
	wsConfig := server.DefaultWSConfig()
	wsConfig.Host = config.Host
	wsConfig.Port = config.Port
	wsConfig.Path = config.WSPath
	if len(config.AllowedOrigins) > 0 {
		wsConfig.AllowedOrigins = config.AllowedOrigins
	}
	if config.AuthToken != "" {
		wsConfig.AuthToken = config.AuthToken
	}

	wsServer := server.NewWSServer(wsConfig, debugger)

	// Handle shutdown gracefully
	_, cancel := context.WithCancel(context.Background())
	shutdownCh := make(chan os.Signal, 1)
	signal.Notify(shutdownCh, syscall.SIGINT, syscall.SIGTERM)

	go func() {
		<-shutdownCh
		log.Println("Shutting down...")
		cancel()
		shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer shutdownCancel()
		if err := wsServer.Shutdown(shutdownCtx); err != nil {
			log.Printf("Server shutdown error: %v", err)
		}
	}()

	// Start server
	log.Printf("Starting WebSocket server on %s:%d%s", config.Host, config.Port, config.WSPath)
	if err := wsServer.Start(); err != nil {
		log.Fatalf("Server error: %v", err)
	}
}

// CLIConfig represents CLI configuration
type CLIConfig struct {
	// Server settings
	Host           string
	Port           int
	WSPath         string
	AllowedOrigins []string
	AuthToken      string

	// Debugger settings
	DebuggerType string
	ProgramPath  string
	ProgramArgs  []string
	WorkingDir   string
	StopOnEntry  bool
	AutoInit     bool
	TimeoutMs    int

	// Output settings
	Quiet       bool
	ShowVersion bool
}

func parseFlags() *CLIConfig {
	config := &CLIConfig{}

	// Server flags
	flag.StringVar(&config.Host, "host", "0.0.0.0", "Server host address")
	flag.IntVar(&config.Port, "port", 8765, "Server port")
	flag.StringVar(&config.WSPath, "ws-path", "/ws", "WebSocket path")
	flag.Var(&stringSliceValue{&config.AllowedOrigins}, "allowed-origins", "Allowed CORS origins (comma-separated)")
	flag.StringVar(&config.AuthToken, "auth-token", "", "Authentication token for clients")

	// Debugger flags
	flag.StringVar(&config.DebuggerType, "debugger", "lldb", "Debugger type (lldb, gdb, shell)")
	flag.StringVar(&config.ProgramPath, "program", "", "Program to debug")
	flag.Var(&stringSliceValue{&config.ProgramArgs}, "args", "Program arguments (comma-separated)")
	flag.StringVar(&config.WorkingDir, "cwd", "", "Working directory")
	flag.BoolVar(&config.StopOnEntry, "stop-on-entry", false, "Stop at program entry")
	flag.BoolVar(&config.AutoInit, "auto-init", true, "Automatically initialize debugger")
	flag.IntVar(&config.TimeoutMs, "timeout", 30000, "Operation timeout (milliseconds)")

	// Output flags
	flag.BoolVar(&config.Quiet, "quiet", false, "Suppress output to stdout")
	flag.BoolVar(&config.ShowVersion, "version", false, "Show version information")

	flag.Parse()

	return config
}

// stringSliceValue implements flag.Value for string slices
type stringSliceValue struct {
	slice *[]string
}

func (v *stringSliceValue) String() string {
	if v.slice == nil {
		return ""
	}
	return fmt.Sprintf("%v", *v.slice)
}

func (v *stringSliceValue) Set(value string) error {
	if *v.slice == nil {
		*v.slice = []string{}
	}
	*v.slice = append(*v.slice, value)
	return nil
}
