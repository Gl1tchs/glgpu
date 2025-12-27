# GLGPU

GLGPU is a high-performance rendering interface that abstracts the Vulkan API. It is designed to provide a low-overhead development environment for real-time graphics applications.

## Features

- **Dynamic Descriptors:** Automated descriptor set allocation and management.
- **Shader Reflection:** Automatic pipeline layout generation using SPIRV-Reflect.
- **Headless Backend:** Support for offscreen rendering and compute-only contexts without a window system.
- **Platform Independent:** Compatible with Windows and Linux.
- **Low-Level API:** explicit control over synchronization, memory barriers, and command submission.
- **SIMD Optimizations:** SIMD intrinsics for internal math operations.

## Prerequisites

To build GLGPU, ensure the following dependencies are installed:

- **CMake** (3.20 or higher)
- **C++20 Compiler** (Preferably Clang)
- **Vulkan SDK** (1.3 or higher)

## Building

### Standalone Build

To build the library, examples, and tests from source:

```bash
git clone --recursive https://github.com/Gl1tchs/glgpu.git
cd glgpu
mkdir build && cd build
cmake ..
cmake --build .
```

## Examples

The `examples/` directory contains reference implementations demonstrating API usage.

If `GL_BUILD_EXAMPLES` is enabled, binaries for these examples will be generated in the build output directory.
