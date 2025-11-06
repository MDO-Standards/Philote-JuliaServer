cmake_minimum_required(VERSION 3.23)
project(PhiloteJuliaServer VERSION 0.0.0 LANGUAGES CXX)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add cmake module path for FindJulia
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Find dependencies
# Find protobuf/gRPC first (required by PhiloteCpp config)
find_package(Protobuf CONFIG QUIET)
if(NOT Protobuf_FOUND)
    find_package(Protobuf REQUIRED)
endif()

find_package(gRPC CONFIG QUIET)
if(NOT gRPC_FOUND)
    find_package(gRPC REQUIRED)
endif()

message(STATUS "Finding Philote-Cpp...")
find_package(PhiloteCpp REQUIRED)

message(STATUS "Finding Julia 1.9+...")
find_package(Julia 1.9 REQUIRED)

message(STATUS "Finding yaml-cpp...")
find_package(yaml-cpp REQUIRED)

# Display found versions
message(STATUS "Julia version: ${Julia_VERSION}")
message(STATUS "Julia executable: ${Julia_EXECUTABLE}")
message(STATUS "Julia include: ${Julia_INCLUDE_DIRS}")
message(STATUS "Julia library: ${Julia_LIBRARY}")

# Library: Julia wrapper implementation
add_library(julia_wrapper
    src/julia_runtime.cpp
    src/julia_thread.cpp
    src/julia_gc.cpp
    src/julia_convert.cpp
    src/julia_config.cpp
    src/julia_executor.cpp
    src/julia_explicit_discipline.cpp
    src/julia_implicit_discipline.cpp
)

target_include_directories(julia_wrapper
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${Julia_INCLUDE_DIRS}
)

target_link_libraries(julia_wrapper
    PUBLIC
        PhiloteCpp::PhiloteCpp
    PRIVATE
        Julia::Julia
        yaml-cpp::yaml-cpp
)

# Set compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(julia_wrapper PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()

# Executable: Server binary
add_executable(philote-julia-serve src/main.cpp)
target_link_libraries(philote-julia-serve PRIVATE julia_wrapper)

# Tests
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Test client executable
add_executable(simple_test_client
    examples/simple_test_client.cpp
)

target_link_libraries(simple_test_client
    PRIVATE
        PhiloteCpp::PhiloteCpp
)

# Installation
install(TARGETS philote-julia-serve
    RUNTIME DESTINATION bin
)

install(TARGETS julia_wrapper
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)
