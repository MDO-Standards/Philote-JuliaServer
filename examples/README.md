# Examples

This directory contains example YAML configurations for Julia disciplines.

## Available Examples

### Paraboloid (Explicit Discipline)

A simple explicit discipline computing a paraboloid function: `f(x, y) = (x-3)^2 + x*y + (y+4)^2 - 3`

**Configuration:** `paraboloid.yaml`

**Run:**
```bash
philote-julia-serve paraboloid.yaml
```

**Inputs:**
- `x`: scalar (default: 1.0)
- `y`: scalar (default: 1.0)

**Outputs:**
- `f`: scalar

### Quadratic (Implicit Discipline)

An implicit discipline solving a quadratic equation: `a*x^2 + b*x + c = 0`

**Configuration:** `quadratic.yaml`

**Run:**
```bash
philote-julia-serve quadratic.yaml
```

**Inputs:**
- `a`, `b`, `c`: coefficients

**Outputs:**
- `x`: solution (one root)

## Configuration Format

All YAML configurations follow this structure:

```yaml
discipline:
  kind: explicit  # or "implicit"
  julia_file: path/to/discipline.jl
  julia_type: DisciplineName
  options: {}  # Optional discipline-specific options

server:
  address: "[::]:50051"  # gRPC server address
  max_threads: 10  # Worker thread pool size
```

### Notes

- `julia_file` paths are resolved relative to the YAML file location
- Use `[::]:PORT` to listen on all interfaces (IPv4 + IPv6)
- Use `localhost:PORT` to listen on localhost only
- `max_threads` limits the gRPC thread pool for predictable Julia thread management

## Connecting Clients

Once the server is running, connect using Philote clients:

**C++ Client:**
```cpp
philote::ExplicitClient client;
auto channel = grpc::CreateChannel("localhost:50051",
                                   grpc::InsecureChannelCredentials());
client.ConnectChannel(channel);
client.Setup();

philote::Variables inputs;
inputs["x"] = philote::Variable({1});
inputs["x"](0) = 2.0;
inputs["y"] = philote::Variable({1});
inputs["y"](0) = 3.0;

philote::Variables outputs = client.ComputeFunction(inputs);
```

**Python Client:**
```python
from philote_mdo.client import ExplicitClient

client = ExplicitClient("localhost:50051")
client.setup()

outputs = client.compute_function({"x": 2.0, "y": 3.0})
print(outputs["f"])
```
