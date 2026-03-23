# Magic Debugger CMake Configuration
# Phase 1: Session Manager

cmake_minimum_required(VERSION 3.16)
project(magic_debugger VERSION 0.1.0 LANGUAGES C)

# C Standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Werror
        -Wpedantic
        -fPIC
    )
endif()

# Define POSIX source
add_definitions(-D_POSIX_C_SOURCE=200809L)

# Find threads
find_package(Threads REQUIRED)

# Source files
set(MD_SOURCES
    src/types.c
    src/logger.c
    src/io_handler.c
    src/signal_handler.c
    src/session_manager.c
)

# Header files
set(MD_HEADERS
    include/types.h
    include/logger.h
    include/io_handler.h
    include/signal_handler.h
    include/session_manager.h
)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Static library
add_library(magic_debugger_static STATIC ${MD_SOURCES})
target_link_libraries(magic_debugger_static PRIVATE Threads::Threads)
set_target_properties(magic_debugger_static PROPERTIES
    OUTPUT_NAME magic_debugger
    POSITION_INDEPENDENT_CODE ON
)

# Shared library (optional)
option(BUILD_SHARED_LIBS "Build shared library" OFF)
if(BUILD_SHARED_LIBS)
    add_library(magic_debugger_shared SHARED ${MD_SOURCES})
    target_link_libraries(magic_debugger_shared PRIVATE Threads::Threads)
    set_target_properties(magic_debugger_shared PROPERTIES
        OUTPUT_NAME magic_debugger
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
endif()

# Test executable
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    
    add_executable(test_session test/test_session.c)
    target_link_libraries(test_session PRIVATE magic_debugger_static Threads::Threads)
    
    add_test(NAME SessionTest COMMAND test_session)
endif()

# Installation
include(GNUInstallDirs)

# Install headers
install(FILES ${MD_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/magic-debugger
)

# Install static library
install(TARGETS magic_debugger_static
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install shared library (if built)
if(BUILD_SHARED_LIBS)
    install(TARGETS magic_debugger_shared
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

# Generate pkg-config file
configure_file(
    ${CMAKE_SOURCE_DIR}/magic_debugger.pc.in
    ${CMAKE_BINARY_DIR}/magic_debugger.pc
    @ONLY
)

install(FILES ${CMAKE_BINARY_DIR}/magic_debugger.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# Print configuration summary
message(STATUS "")
message(STATUS "Magic Debugger ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  Shared library: ${BUILD_SHARED_LIBS}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "")
