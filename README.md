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

## Examples

The `examples/` directory contains reference implementations demonstrating API usage.

If `GPUKIT_BUILD_EXAMPLES` is enabled, binaries for these examples will be generated in the build output directory.
