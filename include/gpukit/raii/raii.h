#pragma once

#include "gpukit/device.h"
#include "gpukit/raii/handle.h"

namespace gpukit::raii {

// shutdown(DeviceHandle) returns void; device.free() always returns {} with no
// error information. Prefer letting Device go out of scope or calling reset().
using Device = UniqueHandle<DeviceHandle, +[](DeviceHandle h) { shutdown(h); }>;

using Buffer = UniqueHandle<gpukit::Buffer, buffer_free>;
using Image = UniqueHandle<gpukit::Image, image_free>;
using Sampler = UniqueHandle<gpukit::Sampler, sampler_free>;
using Shader = UniqueHandle<gpukit::Shader, shader_free>;
using Pipeline = UniqueHandle<gpukit::Pipeline, pipeline_free>;
using UniformSet = UniqueHandle<gpukit::UniformSet, uniform_set_free>;

using RenderPass = UniqueHandle<gpukit::RenderPass, render_pass_destroy>;
using FrameBuffer = UniqueHandle<gpukit::FrameBuffer, frame_buffer_destroy>;

using Swapchain = UniqueHandle<gpukit::Swapchain, swapchain_free>;
using QueryPool = UniqueHandle<gpukit::QueryPool, query_pool_free>;
using CommandPool = UniqueHandle<gpukit::CommandPool, command_pool_free>;

using Fence = UniqueHandle<gpukit::Fence, fence_free>;
using Semaphore = UniqueHandle<gpukit::Semaphore, semaphore_free>;

// CommandBuffer has no RAII wrapper — command buffers are freed implicitly
// when their owning CommandPool is destroyed.

} // namespace gpukit::raii
