# Magic Debugger Makefile
# Phase 1-6: Session Manager, DAP Client, State Model, Adapter Layer, Frontend, AI Interface

# Compiler and flags
CC := gcc
CFLAGS := -Wall -Wextra -Werror -std=c11
CFLAGS += -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
CFLAGS += -fPIC -g -O0
LDFLAGS := -pthread

# ncurses library for TUI (optional)
NCURSES_LIBS := -lncurses -lpanel
TUI_LDFLAGS := $(LDFLAGS) $(NCURSES_LIBS)

# Directories
SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
TEST_DIR := test
VIM_DIR := vim
AI_DIR := ai-interface

# Source files (excluding tui for core library)
CORE_SOURCES := $(filter-out $(SRC_DIR)/tui%.c, $(wildcard $(SRC_DIR)/*.c))
CORE_OBJECTS := $(CORE_SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# TUI source files
TUI_SOURCES := $(wildcard $(SRC_DIR)/tui*.c)
TUI_OBJECTS := $(TUI_SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# All objects
ALL_OBJECTS := $(CORE_OBJECTS) $(TUI_OBJECTS)

# Libraries
LIB_NAME := libmagic_debugger.a
LIB_PATH := $(BUILD_DIR)/$(LIB_NAME)

LIB_TUI_NAME := libmagic_debugger_tui.a
LIB_TUI_PATH := $(BUILD_DIR)/$(LIB_TUI_NAME)

# Test executables
TEST_SESSION_BIN := $(BIN_DIR)/test_session
TEST_STATE_MODEL_BIN := $(BIN_DIR)/test_state_model
TEST_ADAPTER_BIN := $(BIN_DIR)/test_adapter
TEST_TUI_BIN := $(BIN_DIR)/test_tui

# Include paths
INCLUDES := -I$(INC_DIR)

# Vim plugin installation directory
VIM_INSTALL_DIR := ~/.vim/pack/magic-debugger/start/magic-debugger

# Default target
.PHONY: all
all: $(LIB_PATH) $(LIB_TUI_PATH) $(TEST_SESSION_BIN) $(TEST_STATE_MODEL_BIN) $(TEST_ADAPTER_BIN) $(TEST_TUI_BIN)

# Create directories
$(OBJ_DIR) $(BIN_DIR):
	@mkdir -p $@

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Create core static library
$(LIB_PATH): $(CORE_OBJECTS)
	@mkdir -p $(BUILD_DIR)
	ar rcs $@ $^
	@echo "Built core library: $@"

# Create TUI static library
$(LIB_TUI_PATH): $(TUI_OBJECTS) $(CORE_OBJECTS)
	@mkdir -p $(BUILD_DIR)
	ar rcs $@ $^
	@echo "Built TUI library: $@"

# Build session test
$(TEST_SESSION_BIN): $(TEST_DIR)/test_session.c $(LIB_PATH) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BUILD_DIR) -lmagic_debugger $(LDFLAGS) -o $@
	@echo "Built test: $@"

# Build state model test
$(TEST_STATE_MODEL_BIN): $(TEST_DIR)/test_state_model.c $(LIB_PATH) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BUILD_DIR) -lmagic_debugger $(LDFLAGS) -o $@
	@echo "Built test: $@"

# Build adapter test
$(TEST_ADAPTER_BIN): $(TEST_DIR)/test_adapter.c $(LIB_PATH) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BUILD_DIR) -lmagic_debugger $(LDFLAGS) -o $@
	@echo "Built test: $@"

# Build TUI test
$(TEST_TUI_BIN): $(TEST_DIR)/test_tui.c $(LIB_TUI_PATH) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BUILD_DIR) -lmagic_debugger_tui $(LDFLAGS) -o $@
	@echo "Built test: $@"

# Run all tests
.PHONY: test
test: $(TEST_SESSION_BIN) $(TEST_STATE_MODEL_BIN) $(TEST_ADAPTER_BIN) $(TEST_TUI_BIN)
	@echo "Running Phase 1-2 tests (Session Manager + DAP Client)..."
	@echo ""
	./$(TEST_SESSION_BIN)
	@echo ""
	@echo "Running Phase 3 tests (State Model)..."
	@echo ""
	./$(TEST_STATE_MODEL_BIN)
	@echo ""
	@echo "Running Phase 4 tests (Adapter Layer)..."
	@echo ""
	./$(TEST_ADAPTER_BIN)
	@echo ""
	@echo "Running Phase 5 tests (Frontend/TUI)..."
	@echo ""
	./$(TEST_TUI_BIN)

# Run individual test suites
.PHONY: test-session
test-session: $(TEST_SESSION_BIN)
	./$(TEST_SESSION_BIN)

.PHONY: test-state-model
test-state-model: $(TEST_STATE_MODEL_BIN)
	./$(TEST_STATE_MODEL_BIN)

.PHONY: test-adapter
test-adapter: $(TEST_ADAPTER_BIN)
	./$(TEST_ADAPTER_BIN)

.PHONY: test-tui
test-tui: $(TEST_TUI_BIN)
	./$(TEST_TUI_BIN)

# Clean
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	@echo "Cleaned build directory"

# Install headers
.PHONY: install-headers
install-headers:
	@echo "Installing headers to /usr/local/include/magic-debugger"
	@mkdir -p /usr/local/include/magic-debugger
	@cp $(INC_DIR)/*.h /usr/local/include/magic-debugger/

# Install libraries
.PHONY: install-lib
install-lib: $(LIB_PATH) $(LIB_TUI_PATH)
	@echo "Installing libraries to /usr/local/lib"
	@cp $(LIB_PATH) /usr/local/lib/
	@cp $(LIB_TUI_PATH) /usr/local/lib/
	@ldconfig || true

# Install Vim plugin
.PHONY: install-vim
install-vim:
	@echo "Installing Vim plugin to $(VIM_INSTALL_DIR)"
	@mkdir -p $(VIM_INSTALL_DIR)
	@cp -r $(VIM_DIR)/* $(VIM_INSTALL_DIR)/
	@echo "Vim plugin installed. Add 'packloadall' to your .vimrc if needed."

# Full install
.PHONY: install
install: install-headers install-lib install-vim
	@echo ""
	@echo "Installation complete!"
	@echo "  Libraries: /usr/local/lib/"
	@echo "  Headers:   /usr/local/include/magic-debugger/"
	@echo "  Vim:       $(VIM_INSTALL_DIR)/"

# Format code (requires clang-format)
.PHONY: format
format:
	@find $(SRC_DIR) $(INC_DIR) $(TEST_DIR) -name "*.c" -o -name "*.h" | xargs clang-format -i

# Check for memory leaks (requires valgrind)
.PHONY: valgrind
valgrind: $(TEST_SESSION_BIN) $(TEST_STATE_MODEL_BIN) $(TEST_ADAPTER_BIN) $(TEST_TUI_BIN)
	valgrind --leak-check=full --show-leak-kinds=all ./$(TEST_SESSION_BIN)
	valgrind --leak-check=full --show-leak-kinds=all ./$(TEST_STATE_MODEL_BIN)
	valgrind --leak-check=full --show-leak-kinds=all ./$(TEST_ADAPTER_BIN)
	valgrind --leak-check=full --show-leak-kinds=all ./$(TEST_TUI_BIN)

# Static analysis (requires cppcheck)
.PHONY: check
check:
	cppcheck --enable=all --std=c11 -I$(INC_DIR) $(SRC_DIR)

# ============================================================================
# Phase 6: AI Interface (Go)
# ============================================================================

# Go command
GO := go

# AI server binary
AI_SERVER_BIN := $(BIN_DIR)/ai-debug-server

# Build AI interface
.PHONY: ai
ai: $(LIB_PATH)
	@echo "Building AI interface..."
	cd $(AI_DIR) && $(GO) mod tidy
	cd $(AI_DIR) && CGO_CFLAGS="-I$(PWD)/$(INC_DIR)" CGO_LDFLAGS="-L$(PWD)/$(BUILD_DIR)" $(GO) build -o ../$(AI_SERVER_BIN) ./cmd/ai-debug-server
	@echo "Built AI debug server: $(AI_SERVER_BIN)"

# Build AI interface with static linking
.PHONY: ai-static
ai-static: $(LIB_PATH)
	@echo "Building AI interface (static)..."
	cd $(AI_DIR) && $(GO) mod tidy
	cd $(AI_DIR) && CGO_CFLAGS="-I$(PWD)/$(INC_DIR)" CGO_LDFLAGS="-L$(PWD)/$(BUILD_DIR)" $(GO) build -ldflags="-extldflags '-static'" -o ../$(AI_SERVER_BIN) ./cmd/ai-debug-server
	@echo "Built AI debug server (static): $(AI_SERVER_BIN)"

# Test AI interface
.PHONY: test-ai
test-ai: $(LIB_PATH)
	@echo "Testing AI interface..."
	cd $(AI_DIR) && CGO_CFLAGS="-I$(PWD)/$(INC_DIR)" CGO_LDFLAGS="-L$(PWD)/$(BUILD_DIR)" $(GO) test -v ./...

# Run AI server
.PHONY: run-ai
run-ai: ai
	@echo "Starting AI debug server..."
	./$(AI_SERVER_BIN) -port 8765

# Install AI interface
.PHONY: install-ai
install-ai: ai
	@echo "Installing AI debug server..."
	@cp $(AI_SERVER_BIN) /usr/local/bin/
	@echo "Installed: /usr/local/bin/ai-debug-server"

# Clean AI build artifacts
.PHONY: clean-ai
clean-ai:
	@rm -f $(AI_SERVER_BIN)
	cd $(AI_DIR) && $(GO) clean
	@echo "Cleaned AI interface"

# Help
.PHONY: help
help:
	@echo "Magic Debugger - Build System (Phase 1-6)"
	@echo ""
	@echo "Targets:"
	@echo "  all              - Build libraries and tests (default)"
	@echo "  test             - Run all tests"
	@echo "  test-session     - Run session manager tests (Phase 1-2)"
	@echo "  test-state-model - Run state model tests (Phase 3)"
	@echo "  test-adapter     - Run adapter tests (Phase 4)"
	@echo "  test-tui         - Run TUI tests (Phase 5)"
	@echo "  test-ai          - Run AI interface tests (Phase 6)"
	@echo "  clean            - Remove build artifacts"
	@echo "  install          - Install libraries, headers, and Vim plugin"
	@echo "  install-headers  - Install headers only"
	@echo "  install-lib      - Install libraries only"
	@echo "  install-vim      - Install Vim plugin only"
	@echo "  install-ai       - Install AI debug server only"
	@echo "  format           - Format source code"
	@echo "  valgrind         - Run memory leak check"
	@echo "  check            - Run static analysis"
	@echo "  ai               - Build AI interface (requires Go)"
	@echo "  run-ai           - Build and run AI debug server"
	@echo "  help             - Show this help"
	@echo ""
	@echo "Libraries:"
	@echo "  libmagic_debugger.a     - Core library (Phase 1-4)"
	@echo "  libmagic_debugger_tui.a - TUI library (Phase 5, requires ncurses)"
	@echo ""
	@echo "AI Interface (Phase 6):"
	@echo "  ai-debug-server         - WebSocket server for AI integration"
	@echo ""
	@echo "Usage:"
	@echo "  ./ai-debug-server -port 8765 -program /path/to/program"
