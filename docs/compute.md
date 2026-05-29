# Compute Module — Tensor / Kernel / Stream

The compute module is an optional high-level layer on top of gpukit for GPU
compute workloads. It eliminates the per-dispatch boilerplate: buffer creation,
descriptor set management, command recording, synchronisation, and cleanup are
handled automatically.

**When to use it:** when you want to run compute shaders without managing
Vulkan state by hand — algorithm prototyping, AI/ML operators, simulation
kernels, data-parallel processing.  
**When to use raw gpukit instead:** when you need fine-grained control over
queue ownership, multi-pass rendering that mixes compute and graphics, or
specialised synchronisation patterns.

---

## CMake

```cmake
target_link_libraries(my_app PRIVATE gpukit::compute)
```

`gpukit::compute` transitively pulls in `gpukit::gpukit`, so you only need one
`target_link_libraries` call.  
The module requires **C++17** or newer; the public headers contain no C++20
features.

---

## Quick Example

```cpp
#include <gpukit/compute/compute.h>

gpukit::init({});

gpukit::Tensor<float> x(1024), y(1024);
x.upload(x_data);   // std::vector<float> or {ptr, count}
y.upload(y_data);

gpukit::Kernel saxpy("shaders/saxpy.comp");
gpukit::Kernel relu ("shaders/relu.comp");

gpukit::Stream stream;
stream.dispatch(saxpy, 1024, x, y);  // y[i] = 2*x[i] + y[i]
stream.dispatch(relu,  1024, y);      // y[i] = max(0, y[i])
stream.sync();                        // submit + wait; barrier inserted automatically

y.download(result.data(), 1024);

gpukit::device_wait();
gpukit::shutdown();
```

See [`examples/06-compute_module.cpp`](../examples/06-compute_module.cpp) for a
fully verified, runnable version.

---

## Tensor\<T\>

`Tensor<T>` is an RAII wrapper around a GPU buffer typed as an array of `T`.

```cpp
// GPU-local (default) — fast device access, staging required for transfers
gpukit::Tensor<float> t(1024);

// CPU-visible — zero-copy on iGPU/APU and ReBAR-enabled dGPUs, maps directly
gpukit::Tensor<float> t(1024, gpukit::TensorMemory::HOST);
```

### Memory types

| `TensorMemory` | GPU bandwidth | CPU access | Best for |
|---|---|---|---|
| `DEVICE` (default) | Maximum | Staging copy | Most kernels |
| `HOST` | Near-maximum on modern hardware | Direct map | Streaming data, readbacks |

On discrete GPUs without ReBAR, `HOST` maps to host-coherent memory (DDR, not
VRAM). On integrated GPUs and when ReBAR is active, the same memory is
DEVICE_LOCAL, so there is no performance penalty.

### Upload and download

```cpp
// From std::vector — VectorView constructor fires automatically
std::vector<float> cpu(1024, 1.0f);
t.upload(cpu);

// From a raw pointer + count
t.upload({ptr, 512});

// Partial upload — only uploads the first 512 elements
std::vector<float> half(512, 2.0f);
t.upload(half);

// Download to an existing buffer
std::vector<float> out(1024);
t.download(out.data(), 1024);
```

`upload` and `download` for `DEVICE` tensors use a temporary staging buffer and
`command_immediate_submit` internally. They are synchronous from the caller's
perspective.

### Move semantics

`Tensor<T>` is move-only (no copy):

```cpp
auto a = gpukit::Tensor<float>(512);
auto b = std::move(a);  // a.handle() is now GL_NULL_HANDLE
```

### Interop with raw gpukit

```cpp
gpukit::Buffer handle = t.handle();  // use in command_bind_uniform_sets etc.
```

---

## Kernel

`Kernel` compiles a GLSL compute shader and creates its `Pipeline`. Construction
is eager — compilation happens in the constructor.

```cpp
gpukit::Kernel k("path/to/shader.comp");  // compile + pipeline creation
```

The file is loaded, compiled to SPIR-V via shaderc, reflected, and linked
immediately. If compilation fails the constructor asserts.

### Move semantics

```cpp
gpukit::Kernel a("shader.comp");
gpukit::Kernel b = std::move(a);  // a.shader() / a.pipeline() become GL_NULL_HANDLE
```

### Shader conventions

Buffers must be declared with `std430` layout and a distinct `binding` index
starting at 0. `Stream` binds tensors to these indices **positionally** — the
first tensor passed to `dispatch` maps to `binding = 0`, the second to
`binding = 1`, and so on.

```glsl
#version 450

layout(local_size_x = 64) in;  // default local size; override with dispatch local_size arg

layout(set = 0, binding = 0, std430) readonly buffer A { float v[]; } a;
layout(set = 0, binding = 1, std430)          buffer B { float v[]; } b;

void main() {
    uint i = gl_GlobalInvocationID.x;
    b.v[i] = a.v[i] * 2.0;
}
```

`set = 0` is required. Only storage buffers are bound through `Stream`; for
samplers or uniform buffers use the raw gpukit descriptor API.

---

## Stream

`Stream` accumulates compute dispatches into a single command buffer and submits
them together on `sync()`. Automatic pipeline barriers are inserted between
dispatches that share a buffer.

```cpp
gpukit::Stream stream;

stream.dispatch(kernel, n, tensors...);        // local_size_x = 64 (default)
stream.dispatch(kernel, n, local_size, tensors...);  // explicit local_size

stream.sync();  // end recording → submit → fence wait → descriptor cleanup
```

After `sync()` the stream is ready for a new batch.

### Dispatch signature

```cpp
// Variadic template — pass Tensor<T> arguments directly
stream.dispatch(k, 1024, a, b, c);

// With explicit local size (must match local_size_x in the shader)
stream.dispatch(k, 1024, 128, a, b);
```

The number of workgroups dispatched is `ceil(n / local_size)`.

### Automatic barriers

Between consecutive dispatches on the same stream, `Stream` inserts a
`VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT` barrier covering all buffers touched in
prior dispatches. This ensures write-after-write and read-after-write hazards
are resolved without manual `command_pipeline_barrier` calls.

```cpp
// Safe: barrier inserted before the second dispatch reads what the first wrote
stream.dispatch(scale, N, data);
stream.dispatch(relu,  N, data);
stream.sync();
```

For workloads with no shared buffers across dispatches, the barrier is
conservative (it covers all tracked buffers, not just the overlapping ones).
Use multiple `Stream` objects for fully independent pipelines.

### Sync and reuse

```cpp
stream.sync();          // submit + wait; resets internal state
stream.dispatch(...);   // safe to start a new batch immediately
stream.sync();
```

`sync()` on a stream with no recorded dispatches is a no-op.

---

## Initialisation for headless compute

The compute module works without a window. Pass an empty `DeviceCreateInfo`:

```cpp
gpukit::init({});
```

For a dedicated compute queue (runs asynchronously with graphics):

```cpp
gpukit::DeviceCreateInfo info = {};
info.required_features = gpukit::DEVICE_FEATURE_DISTINCT_COMPUTE_QUEUE_BIT;
gpukit::init(info);
```

`Stream` automatically uses the compute queue when one is available and falls
back to the graphics queue otherwise.

---

## Resource Lifetime

`Tensor`, `Kernel`, and `Stream` are RAII objects that free their GPU resources
in their destructors. Because gpukit's allocator is torn down by `shutdown()`,
**all compute objects must be destroyed before `device_wait()` / `shutdown()`
are called**.

The idiomatic pattern is a nested scope:

```cpp
gpukit::init({});

{
    gpukit::Tensor<float> t(N);
    gpukit::Kernel k("shader.comp");
    gpukit::Stream s;
    // ... work ...
} // destructors fire here — buffers, pipelines freed

gpukit::device_wait();
gpukit::shutdown();
```

Alternatively, call the compute objects' destructors explicitly or use
`std::unique_ptr` with a custom deleter.

---

## Performance Notes

| Concern | What Stream does | What to do for more control |
|---|---|---|
| Descriptor sets | Created per `dispatch`, freed on `sync` | Cache pipelines across frames; reuse `Stream` |
| Barriers | Conservative: all tracked buffers | Use separate `Stream` per independent pipeline |
| Transfer | Staging alloc per `upload`/`download` on DEVICE tensors | Use `HOST` tensors for streaming; reuse staging buffers manually |
| Queue submission | One `vkQueueSubmit` per `sync` | Batch more dispatches before calling `sync` |
