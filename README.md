# Philote-JuliaServer

A C++ gRPC server for hosting Julia disciplines in the Philote MDO framework.

## Overview

Philote-JuliaServer enables Julia-based analysis disciplines to be hosted via gRPC, allowing them to integrate seamlessly with MDO (Multidisciplinary Design Optimization) frameworks. This server leverages the Philote-Cpp library for all gRPC infrastructure and protocol handling, focusing solely on Julia integration.

## Features

- **Thread-Safe Julia Integration**: Uses Julia 1.9+ thread adoption for safe concurrent execution
- **Full Philote Support**: Both explicit and implicit disciplines
- **YAML Configuration**: Simple configuration files define disciplines and server settings
- **Leverages Philote-Cpp**: Reuses all gRPC, protobuf, and discipline infrastructure

## Prerequisites

- **Julia 1.9+**: Required for thread adoption support
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

This implementation uses Julia 1.9+ thread adoption to safely call Julia from gRPC worker threads:

1. Julia runtime initialized once on main thread before server starts
2. Each gRPC worker thread adopts itself on first use
3. Mutex serialization ensures thread-safe Julia calls
4. Thread pool size limited via `max_threads` configuration

## License

Apache License 2.0 - See LICENSE file for details.

## Project Status

🚧 **Under Development** - Initial implementation in progress.
