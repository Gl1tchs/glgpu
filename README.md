# GLGPU

GLGPU is a rendering interface abstracting the Vulkan API. It is used to develop low overhead applications.

## Features

- Dynamic descriptions / uniforms
- Shader reflection using SPIRV-Reflect
- Headless backend
- Platform independent
- Low-Level API

## Usage

For example usage check testbed and demo applications under [testbed/](testbed/)

## Building

Embedding into your own project:
sw
```bash
git clone --recursive https://github.com/Gl1tchs/glgpu.git
```

```cmake
set(GL_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
add_subdirectory(glgpu)

target_link_libraries(target PUBLIC glgpu)
```
