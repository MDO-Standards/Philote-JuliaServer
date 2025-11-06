# Philote-JuliaServer

[![Build and Tests](https://github.com/MDO-Standards/Philote-JuliaServer/actions/workflows/build.yml/badge.svg)](https://github.com/MDO-Standards/Philote-JuliaServer/actions/workflows/build.yml)

A C++ gRPC server for hosting Julia disciplines in the Philote MDO framework.

## Overview

Philote-JuliaServer enables Julia-based analysis disciplines to be hosted via gRPC, allowing them to integrate seamlessly with MDO (Multidisciplinary Design Optimization) frameworks. This server leverages the Philote-Cpp library for all gRPC infrastructure and protocol handling, focusing solely on Julia integration.

## Features

- **Thread-Safe Julia Integration**: Uses Julia 1.11+ thread adoption for safe concurrent execution
- **Full Philote Support**: Both explicit and implicit disciplines
- **YAML Configuration**: Simple configuration files define disciplines and server settings
- **Leverages Philote-Cpp**: Reuses all gRPC, protobuf, and discipline infrastructure

## Prerequisites

- **Julia 1.11+**: Required for modern C API with typed array data access
- **Philote-Cpp**: Must be installed and available to CMake
- **C++20 Compiler**: gcc-12+, clang-16+, or MSVC 19.34+
- **CMake 3.23+**
- **yaml-cpp**: For configuration parsing

## Building

```bash
# Create build directory
mkdir build && cd build

# Configure (Philote-Cpp must be installed or discoverable)
cmake ..

# Build
cmake --build .

# Install (optional)
cmake --build . --target install
```

### Specifying Philote-Cpp Location

If Philote-Cpp is installed in a custom location:

```bash
cmake .. -DPhiloteCpp_DIR=/path/to/PhiloteCpp/lib/cmake/PhiloteCpp
```

## Usage

```bash
philote-julia-serve <config.yaml>
```

### Configuration File Format

```yaml
discipline:
  kind: explicit  # or "implicit"
  julia_file: /path/to/discipline.jl
  julia_type: DisciplineName
  options: {}  # Optional discipline-specific options

server:
  address: "[::]:50051"
  max_threads: 10  # Thread pool limit
```

## Examples

See `examples/` directory for sample configurations:
- `paraboloid.yaml` - Explicit discipline example
- `quadratic.yaml` - Implicit discipline example

## Thread Safety

**IMPORTANT**: Julia does NOT support concurrent execution from multiple threads. This implementation uses a **single-threaded executor pattern**:

1. Julia runtime initialized once on main thread before server starts
2. A dedicated executor thread is created and adopted by Julia using `jl_adopt_thread()`
3. **ALL** Julia calls are serialized through this single executor thread (NO CONCURRENCY)
4. gRPC worker threads submit tasks to the executor queue and block until completion
5. Thread pool size (`max_threads`) only affects gRPC network I/O, not Julia execution

This ensures Julia is never called from multiple threads concurrently, which would cause undefined behavior.

## Julia Discipline Interface

### Variable Naming Restrictions

**IMPORTANT**: Variable names (inputs/outputs) **CANNOT contain the tilde character (`~`)**, as it is used as a delimiter in the partials encoding format. This is a limitation of the current implementation.

### Partials Format and Registration

#### Automatic Partials Registration

The server **automatically declares partials** for all output-input pairs during `setup!()`. This means:
- Partials metadata (shape, etc.) is computed based on variable shapes
- The client receives this metadata and preallocates storage
- You don't need to manually declare which partials exist

#### compute_partials() Return Format

The `compute_partials` function must return a flat dictionary with encoded keys:
- Format: `Dict{String, Vector{Float64}}`
- Keys use format: `"output~input"` (e.g., `"y~x"` for ∂y/∂x)
- The array shape must match the shape computed from output and input variable shapes
- Example:
  ```julia
  function compute_partials(discipline, inputs)
      # Return partials as flat dict with encoded keys
      # For scalar output "lift" and scalar input "alpha": ∂lift/∂alpha is a scalar [1] shape
      # For vector output "forces" [3] and scalar input "alpha": ∂forces/∂alpha is a vector [3] shape
      # For scalar output "lift" and vector input "state" [5]: ∂lift/∂state is a vector [5] shape
      return Dict(
          "lift~alpha" => [∂lift_∂alpha],           # scalar ∂/∂ scalar = scalar
          "drag~alpha" => [∂drag_∂alpha],           # scalar ∂/∂ scalar = scalar
          "lift~velocity" => [∂lift_∂velocity]      # scalar ∂/∂ scalar = scalar
      )
  end
  ```

**Rationale**: Nested dict creation in Julia (e.g., `Dict("y" => Dict("x" => [1.0]))`) causes hangs when called from C++ via `jl_call()` in the single-threaded executor pattern. Using flat dicts with encoded keys avoids this issue.

## Testing

Build and run tests:

```bash
cd build
ctest --output-on-failure
```

Test executables:
- `julia_tests` - Unit tests for Julia integration components
- `julia_integration_tests` - End-to-end integration tests

## Current Status

### ✅ Implemented and Working

- **Single-threaded executor pattern** - All Julia calls serialized on dedicated thread
- **Thread adoption** - Executor thread properly adopted by Julia runtime
- **Error handling** - Julia exceptions properly caught and reported with full stack traces
- **gRPC infrastructure** - Server startup, configuration loading, graceful shutdown
- **Julia C API integration** - Runtime initialization, file loading, exception handling
- **Global GC rooting** - Julia objects safely stored across thread boundaries

### ⚠️ Known Issues

1. **Julia module loading via C API** - When loading Julia files that use `Pkg.activate()`, there's a MethodError related to path resolution. The same files load successfully when run directly with Julia. This appears to be related to how `@__DIR__` is resolved when `include()` is called via the C API versus the Julia REPL.

   **Workaround needed**: Julia discipline files currently cannot use `using Philote` when loaded via C++ `include()`. Need to investigate alternative loading mechanisms (possibly `Base.load_path_setup_code()` or direct `jl_eval_string`).

2. **Testing** - Integration tests are not yet complete pending resolution of the module loading issue.

### 🔧 Next Steps

1. Resolve Julia module path resolution when loading via C API
2. Complete integration test suite
3. Test with Python client
4. Add support for implicit disciplines
5. Performance testing and optimization

## Debugging

Enable debug output by examining server logs. The server provides detailed logging:
- `[EXECUTOR]` - Executor thread activity
- `[DEBUG]` - General debug information
- `[Julia Error]` - Full Julia exception details

## License

Apache License 2.0 - See LICENSE file for details.
