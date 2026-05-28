# GPUKit

GPUKit is a high-performance rendering interface that abstracts the Vulkan API. It is designed to provide a low-overhead development environment for real-time graphics applications.

## Features

- **Dynamic Descriptors:** Automated descriptor set allocation and management.
- **Shader Reflection:** Automatic pipeline layout generation using SPIRV-Reflect.
- **Headless Backend:** Support for offscreen rendering and compute-only contexts without a window system.
- **Platform Independent:** Compatible with Windows and Linux.
- **Low-Level API:** explicit control over synchronization, memory barriers, and command submission.
- **SIMD Optimizations:** SIMD intrinsics for internal math operations.

## Prerequisites

To build GPUKit, ensure the following dependencies are installed:

- **CMake** (3.17 or higher)
- **C++20 Compiler** (Preferably Clang)
- **Vulkan SDK** (1.3 or higher)
- **glslc** For runtime SPIRV compilation

## Building

### Standalone Build

To build the library, examples, and tests from source:

```bash
git clone --recursive https://github.com/Gl1tchs/gpukit.git
cd gpukit
cmake --preset release
cmake --build --preset release
```

## Installing

If you are on arch linux you can use AUR to automatically build and install gpukit

```
yay -S gpukit-git
```

Or you can use CMake install command

```
cmake --install build # after building
```

## Documentation

- **[Architecture & API Guide](docs/architecture.md)** — core concepts, resource lifecycle, pipelines, synchronization, and swapchain usage with code examples.
- **[Contributing Guide](CONTRIBUTING.md)** — build presets, commit conventions, test requirements, and PR checklist.

## Examples

The `examples/` directory contains reference implementations demonstrating API usage.

| Example | What it shows |
|---------|--------------|
| `01-hello_gpukit.cpp` | Device init, swapchain, clear-screen frame loop |
| `02-compute_example.cpp` | Headless compute dispatch + CPU readback |
| `03-imgui_example.cpp` | Dear ImGui integration |
| `04-hello_triangle.cpp` | Graphics pipeline, vertex buffers, indexed draw |
| `05-bindless_example.cpp` | Bindless descriptor arrays |

If `GPUKIT_BUILD_EXAMPLES` is enabled, binaries are generated in the build output directory.
