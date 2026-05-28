# GPUKit Architecture & API Guide

GPUKit is a thin C++ abstraction over Vulkan. It keeps Vulkan's explicit control
model intact while removing the most repetitive boilerplate: descriptor pool
management, pipeline layout generation, shader reflection, and memory allocation.

---

## Core Concepts

### Handles

Every GPU resource is represented by an opaque pointer handle:

```cpp
Buffer  buf   = buffer_create(...).value();
Image   img   = image_create(...).value();
Shader  sh    = shader_create(...).value();
// ...
```

Handles that own GPU memory must be freed explicitly:

```cpp
buffer_free(buf);
image_free(img);
shader_free(sh);
```

`GL_NULL_HANDLE` (`nullptr`) is the sentinel for an invalid handle.  
**Dispatchable** handles (`CommandBuffer`, `CommandQueue`) are not freed by the
caller — they are managed by their parent pool or the device.

### `Res<T>` — Error Handling

Every fallible API call returns `Res<T>`, which is an alias for `Result<T, Error>`:

```cpp
auto res = buffer_create(1024, BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::CPU);
if (!res) {
    // res.error() is a gpukit::Error enum value
    fprintf(stderr, "Failed: %d\n", (int)res.error());
}
Buffer buf = res.value(); // only safe after checking is_ok()
```

Common patterns:

```cpp
auto res = buffer_create(...);
REQUIRE(res.is_ok());          // in tests
Buffer buf = res.value();

// Chaining with early return
auto buf = buffer_create(...);
if (!buf) return buf.error();
```

`Res<>` (void specialization) signals success/failure without a value.

---

## Initialization

```cpp
gpukit::DeviceCreateInfo info = {};
info.required_features = gpukit::DEVICE_FEATURE_VALIDATION_LAYERS;
// For windowed rendering, add surface handles:
// info.required_features |= gpukit::DEVICE_FEATURE_SWAPCHAIN_BIT
//                        | gpukit::DEVICE_FEATURE_ENSURE_SURFACE_SUPPORT;
// info.native_connection_handle = display;   // X11: Display*  Win32: HINSTANCE
// info.native_window_handle     = window;    // X11: Window    Win32: HWND

gpukit::init(info);
// ... use the library ...
gpukit::device_wait(); // wait for all GPU work to finish
gpukit::shutdown();
```

For headless compute (no window), omit surface fields entirely.  
Platform-specific window handle extraction is handled by the glue headers:
`gpukit_sdl2_glue.h`, `gpukit_glfw_glue.h`.

---

## Buffers

```cpp
// CPU-visible (host-coherent): suitable for uploads and readbacks
auto buf = buffer_create(size, BUFFER_USAGE_STORAGE_BUFFER_BIT,
                         MemoryAllocationType::CPU).value();

// GPU-local (device memory): fastest for GPU access, not CPU-mappable
auto vbuf = buffer_create(size,
                           BUFFER_USAGE_VERTEX_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT,
                           MemoryAllocationType::GPU).value();
```

**CPU buffers** can be mapped directly:

```cpp
uint8_t* ptr = buffer_map(buf).value();
memcpy(ptr, data, size);
buffer_flush(buf);   // CPU → GPU cache sync
buffer_unmap(buf);
```

`buffer_upload(buf, data, size)` is a convenience wrapper that does the
map/copy/flush/unmap in one call.

`buffer_invalidate(buf)` syncs GPU writes back to CPU (needed before reading a
buffer that the GPU has written).

---

## Images and Samplers

```cpp
ImageCreateInfo info = {};
info.size    = { 512, 512 };
info.format  = DataFormat::R8G8B8A8_UNORM;
info.usage   = IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_TRANSFER_DST_BIT;
info.samples = 1;

Image img = image_create(info).value();

// Upload pixel data (uses an internal staging buffer + immediate submit)
image_upload(img, pixels, byte_size);
```

**Depth images** use a depth format and the depth-stencil usage flag:

```cpp
info.format = DataFormat::D32_SFLOAT;
info.usage  = IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
```

**Mipmapped images** set `mipmapped = true` and include `TRANSFER_SRC_BIT` so
`command_generate_mipmaps` can blit between levels.

**Samplers** are separate objects and are combined with images at descriptor
bind time:

```cpp
SamplerCreateInfo sinfo = {};
sinfo.min_filter = ImageFiltering::LINEAR;
sinfo.mag_filter = ImageFiltering::LINEAR;
sinfo.wrap_u = sinfo.wrap_v = sinfo.wrap_w = ImageWrappingMode::REPEAT;

Sampler sampler = sampler_create(sinfo).value();
```

---

## Shaders and Pipelines

### Creating Shaders

From GLSL source files (compiled at runtime via shaderc or `glslc`):

```cpp
Shader shader = shader_create("path/to/shader.vert", "path/to/shader.frag").value();
Shader compute = shader_create("path/to/shader.comp").value();
```

From pre-compiled SPIR-V:

```cpp
SpirvEntry vert = { spirv_bytes, SHADER_STAGE_VERTEX_BIT };
SpirvEntry frag = { spirv_bytes, SHADER_STAGE_FRAGMENT_BIT };
Shader shader = shader_create({ vert, frag }).value();
```

**Shader reflection** is performed automatically. Query the reflected layout:

```cpp
auto resources = shader_get_resources(shader).value();
for (auto& r : resources) {
    // r.set, r.binding, r.type (ShaderUniformType), r.name
}
auto vertex_inputs = shader_get_vertex_inputs(shader).value();
```

### Graphics Pipelines

```cpp
GraphicsPipelineCreateInfo pci = {};
pci.shader    = shader;
pci.primitive = RenderPrimitive::TRIANGLE_LIST;
pci.rasterization_state.cull_mode = PolygonCullMode::BACK;
pci.multisample_state.sample_count = 1;
pci.depth_stencil_state.enable_depth_test  = true;
pci.depth_stencil_state.enable_depth_write = true;
pci.depth_stencil_state.depth_compare_operator = CompareOperator::LESS_OR_EQUAL;
pci.color_blend_state = PipelineColorBlendState::create_disabled(1);

// For dynamic rendering (no legacy render pass):
pci.rendering_info.color_attachments = { DataFormat::R8G8B8A8_UNORM };
pci.rendering_info.depth_attachment  = DataFormat::D32_SFLOAT;

// For legacy render pass:
// pci.render_pass = rp;

Pipeline pipeline = graphics_pipeline_create(pci).value();
```

### Compute Pipelines

```cpp
Pipeline pipeline = compute_pipeline_create(shader).value();

// With specialization constants:
std::vector<SpecializationConstant> consts = {
    SpecializationConstant::from_uint(0, 128), // override constant_id=0
};
Pipeline pipeline = compute_pipeline_create(shader, consts).value();
```

---

## Uniform Sets (Descriptors)

GPUKit exposes two descriptor binding models.

### Explicit binding (one set per call)

```cpp
// Empty set — update individual bindings after creation
UniformSet set = uniform_set_create(shader, 0).value();
uniform_set_update_texture(set, 0, 0, img, sampler);
uniform_set_update_buffer(set, 1, 0, buf);

// Pre-populated set
ShaderUniform uniform;
uniform.type    = ShaderUniformType::STORAGE_BUFFER;
uniform.binding = 0;
uniform.data    = { (void*)buf }; // one void* per array element
UniformSet set  = uniform_set_create(uniform, shader, 0).value();
```

### Bindless (large descriptor array)

Bindless sets hold up to `max_count` resources and update them by array index:

```cpp
// set=0, binding=0, up to 1000 entries
UniformSet bindless = uniform_set_create_bindless(shader, 0, 0, 1000).value();
uniform_set_update_texture(bindless, 0, texture_index, img, sampler);
uniform_set_update_buffer(bindless, 0, buffer_index, buf);
```

Access in GLSL with `GL_EXT_nonuniform_qualifier`:

```glsl
layout(set = 0, binding = 0) uniform sampler2D textures[];
vec4 c = texture(textures[nonuniformEXT(index)], uv);
```

---

## Command Recording

Commands are recorded into `CommandBuffer`s allocated from a `CommandPool`.

```cpp
CommandQueue queue = queue_get(QueueType::GRAPHICS).value();
CommandPool  pool  = command_pool_create(queue).value();
CommandBuffer cmd  = command_pool_allocate(pool).value();
```

### Frame pattern

```cpp
command_begin(cmd);

// Transition -> render -> transition
command_transition_image(cmd, color, ImageLayout::UNDEFINED,
                          ImageLayout::COLOR_ATTACHMENT_OPTIMAL);

RenderingAttachment att = {};
att.image     = color;
att.layout    = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
att.load_op   = AttachmentLoadOp::CLEAR;
att.store_op  = AttachmentStoreOp::STORE;
att.clear_color = COLOR_BLACK;

command_begin_rendering(cmd, { W, H }, att, depth_img);
    command_bind_graphics_pipeline(cmd, pipeline);
    command_set_viewport(cmd, { W, H });
    command_set_scissor(cmd, { W, H });
    command_bind_uniform_sets(cmd, shader, 0, uniform_set);
    command_draw(cmd, 3, 1);
command_end_rendering(cmd);

command_end(cmd);

Fence fence = fence_create(false);
queue_submit(queue, cmd, fence);
fence_wait(fence);
```

`command_pool_reset` resets all buffers allocated from the pool at once.
`command_reset` resets a single command buffer.

---

## Synchronization

### Fences — CPU/GPU sync

```cpp
Fence fence = fence_create(false);           // unsignaled
queue_submit(queue, cmd, fence);
fence_wait(fence);                           // blocks CPU until GPU done
fence_reset(fence);                          // rearm for next frame
```

`fence_create(true)` creates a pre-signaled fence (useful for the first frame).

### Binary Semaphores — GPU/GPU sync

```cpp
Semaphore signal = semaphore_create();

queue_submit(queue, cmd1, GL_NULL_HANDLE, GL_NULL_HANDLE, signal); // signals
queue_submit(queue, cmd2, fence, signal, GL_NULL_HANDLE);          // waits
```

### Timeline Semaphores — ordered GPU/CPU sync

Timeline semaphores carry a monotonically increasing counter, enabling
fine-grained CPU/GPU coordination without one fence per operation:

```cpp
Semaphore timeline = timeline_semaphore_create(0).value();

// GPU signals value 1 on completion
queue_submit(queue, cmd,
    SemaphoreSubmitInfo{},                              // no wait
    SemaphoreSubmitInfo{ .semaphore = timeline, .value = 1 });

// CPU waits for GPU to reach value 1
semaphore_wait(timeline, 1);

// CPU can also signal (useful for frame pacing)
semaphore_signal(timeline, 2);
```

---

## Render Pass Models

### Dynamic Rendering (recommended)

No render pass object required. Attachment formats are baked into the pipeline
via `GraphicsPipelineCreateInfo::rendering_info`:

```cpp
command_begin_rendering(cmd, extent, color_attachments, depth_image);
// draw calls
command_end_rendering(cmd);
```

### Legacy Render Pass

For compatibility with older Vulkan patterns:

```cpp
RenderPassAttachment att = { DataFormat::R8G8B8A8_UNORM, ... };
SubpassInfo subpass = { { SubpassAttachment{ 0, SUBPASS_ATTACHMENT_COLOR } } };
RenderPass rp = render_pass_create(att, subpass).value();

FrameBuffer fb = frame_buffer_create(rp, color_image, { W, H }).value();

command_begin_render_pass(cmd, rp, fb, { W, H }, COLOR_BLACK);
// draw calls
command_end_render_pass(cmd);
```

Pipeline must be created with `pci.render_pass = rp` when using the legacy path.

---

## Swapchain

Requires `DEVICE_FEATURE_SWAPCHAIN_BIT` and a valid native surface handle at
`init()` time.

```cpp
Swapchain swapchain = swapchain_create().value();
swapchain_resize(queue, swapchain, { W, H }, /*vsync=*/false);

// Per-frame acquire/present loop:
Semaphore image_ready = semaphore_create();

Image frame_img = swapchain_acquire_image(swapchain, image_ready).value();
// record commands for frame_img ...

Semaphore render_done = semaphore_create();
queue_submit(queue, cmd, fence, image_ready, render_done);
queue_present(present_queue, swapchain, render_done);
```

---

## Query Pools (GPU Timestamps)

```cpp
QueryPool qpool = query_pool_create(2).value();

command_begin(cmd);
command_reset_query_pool(cmd, qpool, 0, 2);
command_write_timestamp(cmd, qpool, 0, PIPELINE_STAGE_TOP_OF_PIPE_BIT);
// ... work ...
command_write_timestamp(cmd, qpool, 1, PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
command_end(cmd);

queue_submit(queue, cmd, fence);
fence_wait(fence);

auto ts = query_pool_get_results(qpool, 0, 2).value();
double elapsed_ns = timestamps_to_ns(ts[1] - ts[0]);
```

---

## Math Utilities

GPUKit bundles a SIMD-accelerated math library (SIMD enabled automatically on
x86/x86_64). Key types:

| Type | Description |
|------|-------------|
| `Vec2f`, `Vec3f`, `Vec4f` | Float vectors |
| `Vec2u`, `Vec3u`, `Vec4u` | Unsigned int vectors |
| `Vec2i`, `Vec3i`, `Vec4i` | Signed int vectors |
| `Mat3f`, `Mat4f` | Column-major matrices |
| `Quaternion` | Unit quaternion |
| `Color` | RGBA float color (`COLOR_BLACK`, `COLOR_WHITE`, etc.) |

---

## Platform Glue

Window-system integration is provided by opt-in headers that are not part of
the core library:

| Header | Window system |
|--------|--------------|
| `gpukit_sdl2_glue.h` | SDL2 |
| `gpukit_glfw_glue.h` | GLFW |
| `gpukit_imgui_glue.h` | Dear ImGui (Vulkan backend) |
| `gpukit_android_glue.h` | Android NDK |

These headers extract the native handles needed by `DeviceCreateInfo` and
provide the ImGui backend wired to the internal Vulkan context.
