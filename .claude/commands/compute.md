# Compute Module — Tensor / Kernel / Stream

You are working on the **high-level compute module** of GPUKit (`gpukit::compute`). This module is the primary API for simulation and scientific workloads.

## Key files

| File | Role |
|---|---|
| `include/gpukit/compute/tensor.h` | `Tensor<T,N>` typed GPU buffer |
| `include/gpukit/compute/kernel.h` | `Kernel` — shader + pipeline RAII |
| `include/gpukit/compute/stream.h` | `Stream` — dispatch accumulator |
| `compute/src/kernel.cpp` | Kernel implementation |
| `include/gpukit/compute/compute.h` | Master include |
| `docs/compute.md` | Full reference doc |

## Tensor<T, N>

- `N=1` (default): element is `T` → maps to GLSL `float data[]`, `int data[]`, etc.
- `N>1`: element is `std::array<T,N>` → maps to GLSL `vec4 data[]`, `ivec3 data[]`, etc.
- `TensorMemory::DEVICE` (default): GPU-local, staging required for CPU transfers
- `TensorMemory::HOST`: CPU-visible, zero-copy on iGPU/ReBAR

```cpp
Tensor<float>    a(N);          // scalar float array
Tensor<float, 4> b(N);          // vec4 array — maps to vec4 data[] in GLSL
Tensor<float> c(N, TensorMemory::HOST);  // host-visible
```

Move-only. `handle()` returns the raw `Buffer` for interop with the core API.

## Kernel

```cpp
Kernel k("path/to/shader.comp");                          // from file
Kernel k2 = Kernel::from_source(glsl_string);             // from inline GLSL
```

- `local_size` defaults to 64 — must match `layout(local_size_x = N)` in the shader
- Construction is eager: compile + pipeline link happen in the constructor
- Move-only; moved-from kernel has `GL_NULL_HANDLE` handles

## Stream

```cpp
Stream s;
s.dispatch(k, a, b, c);   // count = a.count(), local_size = k.local_size()
s.sync();                  // submit + wait; resets internal state
```

- Tensors are bound **positionally** to `binding=0,1,...` in the shader
- Automatic `VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT` barrier between dispatches sharing a buffer
- `sync()` is a no-op on an empty stream
- Uses the dedicated compute queue when available, falls back to graphics queue

## GLSL shader conventions

```glsl
#version 450
layout(local_size_x = 64) in;

layout(set = 0, binding = 0, std430) readonly buffer A { float v[]; } a;
layout(set = 0, binding = 1, std430)         buffer B { float v[]; } b;

void main() {
    uint i = gl_GlobalInvocationID.x;
    b.v[i] = a.v[i] * 2.0;
}
```

Rules:
- `set = 0` is required
- `std430` layout required for all storage buffers
- `local_size_x` must match `Kernel::local_size()`
- Use `readonly` for input buffers — enables better GPU optimization

## Headless init for compute-only workloads

```cpp
gpukit::init({});                          // minimal, no window
// or with dedicated compute queue:
gpukit::DeviceCreateInfo info = {};
info.required_features = gpukit::DEVICE_FEATURE_DISTINCT_COMPUTE_QUEUE_BIT;
gpukit::init(info);
```

## Object lifetime

All compute objects must be destroyed before `device_wait()` / `shutdown()`:

```cpp
gpukit::init({});
{
    Tensor<float> t(N);
    Kernel k("shader.comp");
    Stream s;
    // work...
} // destructors here
gpukit::device_wait();
gpukit::shutdown();
```

## Common tasks

- **Add a new kernel**: create a `.comp` file in the appropriate location, construct a `Kernel`, dispatch via `Stream`
- **Multi-pass pipeline**: chain `stream.dispatch(k1, ...)` then `stream.dispatch(k2, ...)` — barrier is auto-inserted if buffers overlap
- **Large grid dispatch**: `stream.dispatch(k, a, b)` dispatches `ceil(a.count() / local_size)` workgroups — make sure the shader guards on `gl_GlobalInvocationID.x < N`
- **CPU readback**: `tensor.download(ptr, count)` is synchronous after `stream.sync()`
- **Per-frame streaming**: use `TensorMemory::HOST` to avoid staging allocation overhead on repeated uploads
