# GPUKit — Claude Context

## Project Overview

**GPUKit** is a C++17 low-overhead Vulkan abstraction library targeting **GPU compute for scientific and engineering simulation workloads**. It is NOT a game engine — the primary use case is headless compute pipelines: physics simulations, fluid dynamics, ML operators, numerical solvers, and data-parallel scientific algorithms.

**Version:** 1.6.1  
**License:** See `LICENSE`  
**Repo:** https://github.com/Gl1tchs/gpukit

---

## Repository Layout

```
gpukit/
├── include/gpukit/          # Public C++ headers
│   ├── gpukit.h             # Master include
│   ├── types.h              # All enums, structs, handle typedefs
│   ├── device.h             # Init/shutdown, device queries
│   ├── compute/             # High-level compute module
│   │   ├── compute.h        # Master compute include
│   │   ├── tensor.h         # Tensor<T,N> — typed GPU buffer wrapper
│   │   ├── kernel.h         # Kernel — shader + pipeline RAII
│   │   └── stream.h         # Stream — dispatch accumulator + auto-barrier
│   ├── math.h / matrix.h / quaternion.h / vector.h  # SIMD math library
│   ├── result.h             # Result<T,E> / Res<T> error type
│   ├── raii/                # RAII handle wrappers
│   └── ...
├── src/                     # Library implementation
├── compute/src/             # Compute module implementation
│   └── kernel.cpp
├── tests/                   # Catch2 test suite (headless Vulkan)
├── examples/                # Reference implementations
├── docs/
│   ├── architecture.md      # Full API guide (read this for deep dives)
│   └── compute.md           # Compute module guide
├── misc/                    # Platform glue headers (SDL2, GLFW, ImGui, Android)
├── external/                # Vendored: SPIRV-Reflect, vma, vk-bootstrap
├── CMakeLists.txt
├── CMakePresets.json        # Presets: debug, release, sanitize, coverage
└── CONTRIBUTING.md
```

---

## Build System

CMake ≥ 3.17, C++17 (Clang preferred), Vulkan SDK ≥ 1.3, `glslc`.

```bash
# Development build
cmake --preset debug && cmake --build --preset debug

# Release
cmake --preset release && cmake --build --preset release

# Sanitizers (ASan + UBSan)
cmake --preset sanitize && cmake --build --preset sanitize

# Coverage (requires lcov + genhtml)
cmake --preset coverage && cmake --build build-cov --target coverage
```

Key CMake options:
- `GPUKIT_BUILD_COMPUTE_MODULE` — builds `gpukit::compute` target (ON by default)
- `GPUKIT_BUILD_TESTS` / `GPUKIT_BUILD_EXAMPLES` — ON by default
- `GPUKIT_ENABLE_COVERAGE` — gcov instrumentation
- `GPUKIT_ENABLE_SANITIZERS` — ASan + UBSan
- `GPUKIT_USE_SIMD_INTRINSICS` — auto-ON on x86/x86_64
- `GPUKIT_ENABLE_SHADER_COMPILATION` — runtime GLSL→SPIR-V via shaderc (ON by default, OFF on Android)

CMake targets:
- `gpukit::gpukit` — core library
- `gpukit::compute` — high-level compute module (pulls in `gpukit::gpukit` transitively)

---

## Core API Patterns

### Error Handling
Every fallible call returns `Res<T>` (`Result<T, Error>`):
```cpp
auto buf = buffer_create(size, BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::CPU);
if (!buf) return buf.error();
Buffer b = buf.value();
// In tests: REQUIRE(buf.is_ok());
```

### Handles
- Opaque pointers; `GL_NULL_HANDLE` is `nullptr`.
- Non-dispatchable handles (Buffer, Image, Pipeline, Shader, UniformSet, Fence, Semaphore, QueryPool, RenderPass, FrameBuffer, Swapchain, CommandPool) must be freed by the caller.
- Dispatchable handles (CommandBuffer, CommandQueue) are pool-managed — do NOT free them.

### Initialization
```cpp
gpukit::init({});           // headless compute
gpukit::device_wait();
gpukit::shutdown();
```
All GPU objects must be destroyed before `device_wait()` / `shutdown()`.

---

## High-Level Compute Module

`#include <gpukit/compute/compute.h>` — link `gpukit::compute`.

**Three classes:**

| Class | Purpose |
|---|---|
| `Tensor<T, N>` | Typed GPU storage buffer, RAII, move-only |
| `Kernel` | Loads GLSL `.comp`, compiles via shaderc, owns Pipeline |
| `Stream` | Accumulates dispatches, auto-inserts pipeline barriers, submits on `sync()` |

```cpp
gpukit::Tensor<float> a(N), b(N);
a.upload(cpu_data);

gpukit::Kernel k("shader.comp");
gpukit::Stream s;
s.dispatch(k, a, b);   // count from first tensor, local_size from kernel
s.sync();

b.download(out.data(), N);
```

GLSL shader convention: `set = 0`, `binding = 0,1,...` positionally matching dispatch args, `std430` layout.

---

## Test Suite

Uses **Catch2 v3**. Located in `tests/`. All tests run headlessly (no window, no swapchain).

```bash
ctest --test-dir build --output-on-failure
```

Key patterns:
- `ensure_test_device()` at top of every `TEST_CASE`
- `REQUIRE(res.is_ok())` before `.value()`
- Explicit resource cleanup at end of test
- Shared helpers in `test_common.h` (`gpukit::test::load_shader()`, `load_compute_shader()`)
- Shader assets in `tests/assets/` (GLSL source, `.spv` is gitignored)

---

## Code Style

- Enforced by `.clang-format` (run `clang-format -i src/**/*.cpp include/**/*.h` before committing)
- Static analysis via `.clang-tidy`
- Commit format: `feat(<module>):` / `fix(<module>):` — one logical change per commit

---

## Simulation Roadmap Context

The project is evolving toward being a **simulation-ready GPU compute framework** for:
- Physics simulations (rigid body, soft body, fluid dynamics)
- Numerical solvers (FEM, FDM, particle methods)
- Scientific computing (ML inference, data-parallel algorithms)
- Engineering workloads (CFD, structural analysis)

The `compute` module (Tensor/Kernel/Stream) is the primary API for simulation code. The goal is to keep the abstraction thin enough for experts to drop to raw Vulkan when needed, while making common patterns (dispatch, sync, readback) zero-boilerplate.

---

## Custom Slash Commands

| Command | Purpose |
|---|---|
| `/project:test-coverage` | Guide for running and improving test coverage |
| `/project:security-review` | Security review for Vulkan/GPU code |
| `/project:code-quality` | clang-format, clang-tidy, sanitizer workflow |
| `/project:compute` | Compute module (Tensor/Kernel/Stream) guidance |
| `/project:sync` | Synchronization primitives reference |
| `/project:simulation` | Adding simulation workloads to the framework |
