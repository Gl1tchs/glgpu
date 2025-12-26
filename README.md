# GLGPU

GLGPU is a rendering interface abstracting the Vulkan API. It is used to develop low overhead applications.

## Features

- Dynamic descriptions / uniforms
- Shader reflection using SPIRV-Reflect
- Headless backend
- Platform independent
- Low-Level API

## Usage

For usage and API references check our examples under [examples/](examples/)

## Building

Embedding into your own project:

```bash
git clone --recursive https://github.com/Gl1tchs/glgpu.git
```

```cmake
set(GL_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
add_subdirectory(glgpu)

target_link_libraries(target PUBLIC glgpu)
```
