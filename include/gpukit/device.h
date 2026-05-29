#pragma once

#include "gpukit/color.h"
#include "gpukit/export.h"
#include "gpukit/types.h"
#include "gpukit/vector_view.h"

#include <functional>

namespace gpukit {

enum DeviceRequiredFeatureBits : uint32_t {
	DEVICE_FEATURE_NONE = 0x0,
	DEVICE_FEATURE_SWAPCHAIN_BIT = 0x1,
	DEVICE_FEATURE_ENSURE_SURFACE_SUPPORT = 0x2,
	DEVICE_FEATURE_DISTINCT_COMPUTE_QUEUE_BIT = 0x4,
	DEVICE_FEATURE_VALIDATION_LAYERS = 0x8,
};
typedef uint32_t DeviceFeatureFlags;

struct DeviceCreateInfo {
	DeviceFeatureFlags required_features = DEVICE_FEATURE_NONE;
	void* native_connection_handle =
			nullptr; // Win32: HINSTANCE | X11: Display* | Wayland: wl_display* | Android: nullptr
	void* native_window_handle =
			nullptr; // Win32: HWND | X11: Window | Wayland: wl_surface* | Android: ANativeWindow*
	uint32_t max_bindless_descriptors = 1000;
	const char* pipeline_cache_path = nullptr; // If null, cache is not persisted to disk
};

/**
 * Opaque bag of raw backend handles for use by glue code (e.g. gpukit_imgui_glue.h).
 * Fields are cast to the appropriate backend types at the point of use.
 * Do NOT depend on this struct in general application code.
 */
struct NativeContext {
	void* instance = nullptr; // VkInstance
	void* physical_device = nullptr; // VkPhysicalDevice
	void* device = nullptr; // VkDevice
	void* graphics_queue = nullptr; // VkQueue
	uint32_t graphics_queue_family = 0;
};

// =========================================================================
// Lifecycle
// =========================================================================

GPUKIT_API Res<DeviceHandle> init(const DeviceCreateInfo& info);
GPUKIT_API void shutdown(DeviceHandle device);
// Shutdown current device
GPUKIT_API void shutdown();
GPUKIT_API void select_device(DeviceHandle device);

// =========================================================================
// Device & Surface
// =========================================================================

GPUKIT_API Res<> device_wait();
GPUKIT_API Res<> attach_surface(void* connection_handle, void* window_handle);
GPUKIT_API uint32_t get_max_msaa_samples();
GPUKIT_API uint32_t get_max_bindless_instances();
GPUKIT_API bool is_swapchain_supported();
GPUKIT_API NativeContext get_native_context();
GPUKIT_API Res<CommandQueue> queue_get(QueueType type);

// =========================================================================
// Swapchain
// =========================================================================

GPUKIT_API Res<Swapchain> swapchain_create();
GPUKIT_API Res<> swapchain_resize(
		CommandQueue cmd_queue, Swapchain swapchain, Vec2u size, bool vsync = false);
GPUKIT_API Res<size_t> swapchain_get_image_count(Swapchain swapchain);
GPUKIT_API Res<std::vector<Image>> swapchain_get_images(Swapchain swapchain);
GPUKIT_API Res<Image> swapchain_acquire_image(
		Swapchain swapchain, Semaphore semaphore, uint32_t* o_image_index = nullptr);
GPUKIT_API Res<Vec2u> swapchain_get_extent(Swapchain swapchain);
GPUKIT_API Res<DataFormat> swapchain_get_format(Swapchain swapchain);
// Returns the VkSurfaceTransformFlagBitsKHR value set at last swapchain_resize.
// Use this to apply the inverse pre-rotation transform in your vertex shader on Android.
GPUKIT_API Res<uint32_t> swapchain_get_pre_transform(Swapchain swapchain);
GPUKIT_API Res<> swapchain_free(Swapchain swapchain);

// =========================================================================
// Resource Management (Buffers & Images)
// =========================================================================

// Buffer

GPUKIT_API Res<Buffer> buffer_create(
		uint64_t size, BufferUsageFlags usage, MemoryAllocationType allocation_type);
GPUKIT_API Res<> buffer_free(Buffer buffer);
GPUKIT_API Res<BufferDeviceAddress> buffer_get_device_address(Buffer buffer);
GPUKIT_API Res<uint8_t*> buffer_map(Buffer buffer);
GPUKIT_API Res<> buffer_unmap(Buffer buffer);
GPUKIT_API Res<> buffer_invalidate(Buffer buffer); // GPU -> CPU
GPUKIT_API Res<> buffer_flush(Buffer buffer); // CPU -> GPU
GPUKIT_API Res<> buffer_upload(Buffer buffer, const void* data, size_t size, size_t offset = 0);

// Image

GPUKIT_API Res<Image> image_create(const ImageCreateInfo& info);
GPUKIT_API Res<> image_free(Image image);
GPUKIT_API Res<> image_upload(Image image, const void* data, size_t size);
GPUKIT_API Res<Vec3u> image_get_size(Image image);
GPUKIT_API Res<DataFormat> image_get_format(Image image);
GPUKIT_API Res<uint32_t> image_get_mip_levels(Image image);
GPUKIT_API Res<ImageUsageFlags> image_get_image_usage(Image image);

// Returns the backend-native image view handle (e.g. VkImageView) as void*.
// For use by glue code only — do not depend on this in general application code.
GPUKIT_API Res<void*> image_get_native_view(Image image);

// Returns the backend-native sampler handle (e.g. VkSampler) as void*.
// For use by glue code only — do not depend on this in general application code.
GPUKIT_API Res<void*> sampler_get_native(Sampler sampler);

// Sampler

GPUKIT_API Res<Sampler> sampler_create(const SamplerCreateInfo& info);
GPUKIT_API Res<> sampler_free(Sampler sampler);

// =========================================================================
// Shader & Pipelines
// =========================================================================

// Create and compile shader from spirv bytecode
GPUKIT_API Res<Shader> shader_create(VectorView<SpirvEntry> shaders);

// Create and compile shader from glsl files
GPUKIT_API Res<Shader> shader_create(const char* vertex_filepath, const char* fragment_filepath);

// Create and compile compute shader from glsl file
GPUKIT_API Res<Shader> shader_create(const char* compute_filepath);

GPUKIT_API Res<> shader_free(Shader shader);
GPUKIT_API Res<std::vector<ShaderInterfaceVariable>> shader_get_vertex_inputs(Shader shader);
GPUKIT_API Res<std::vector<ShaderResourceInfo>> shader_get_resources(Shader shader);

GPUKIT_API Res<Pipeline> graphics_pipeline_create(const GraphicsPipelineCreateInfo& info);
GPUKIT_API Res<Pipeline> compute_pipeline_create(Shader shader);
GPUKIT_API Res<Pipeline> compute_pipeline_create(
		Shader shader, VectorView<SpecializationConstant> specialization_constants);
GPUKIT_API Res<> pipeline_free(Pipeline pipeline);

// Descriptors / Uniforms

GPUKIT_API Res<UniformSet> uniform_set_create(
		VectorView<ShaderUniform> uniforms, Shader shader, uint32_t set_index);
// Create an empty uniform set
GPUKIT_API Res<UniformSet> uniform_set_create(Shader shader, uint32_t set_index);
GPUKIT_API Res<> uniform_set_free(UniformSet uniform_set);

GPUKIT_API Res<UniformSet> uniform_set_create_bindless(
		Shader shader, uint32_t set_index, uint32_t binding_index, uint32_t max_count);

GPUKIT_API Res<> uniform_set_update_texture(
		UniformSet set, uint32_t binding, uint32_t array_index, Image image, Sampler sampler);
GPUKIT_API Res<> uniform_set_update_sampled_image(
		UniformSet set, uint32_t binding, uint32_t array_index, Image image);
GPUKIT_API Res<> uniform_set_update_storage_image(
		UniformSet set, uint32_t binding, uint32_t array_index, Image image);
GPUKIT_API Res<> uniform_set_update_buffer(
		UniformSet set, uint32_t binding, uint32_t array_index, Buffer buffer);
GPUKIT_API Res<> uniform_set_update_buffer_range(UniformSet set, uint32_t binding,
		uint32_t array_index, Buffer buffer, uint64_t offset, uint64_t range);

// =========================================================================
// Render Pass & Framebuffer (Legacy)
// =========================================================================

GPUKIT_API Res<RenderPass> render_pass_create(
		VectorView<RenderPassAttachment> attachments, VectorView<SubpassInfo> subpasses);
GPUKIT_API Res<> render_pass_destroy(RenderPass render_pass);

GPUKIT_API Res<FrameBuffer> frame_buffer_create(
		RenderPass render_pass, VectorView<Image> attachments, const Vec2u& extent);
GPUKIT_API Res<> frame_buffer_destroy(FrameBuffer frame_buffer);

// =========================================================================
// Query Pools
// =========================================================================

GPUKIT_API Res<QueryPool> query_pool_create(uint32_t count);
GPUKIT_API Res<> query_pool_free(QueryPool pool);
GPUKIT_API Res<std::vector<uint64_t>> query_pool_get_results(
		QueryPool pool, uint32_t first = 0, uint32_t count = UINT32_MAX);
GPUKIT_API double timestamps_to_ns(uint64_t ticks);

// =========================================================================
// Synchronization
// =========================================================================

GPUKIT_API Fence fence_create(bool create_signaled = true);
GPUKIT_API Res<> fence_free(Fence fence);
GPUKIT_API Res<> fence_wait(Fence fence);
GPUKIT_API Res<> fence_reset(Fence fence);

GPUKIT_API Semaphore semaphore_create();
GPUKIT_API Res<Semaphore> timeline_semaphore_create(uint64_t initial_value = 0);
GPUKIT_API Res<> semaphore_free(Semaphore semaphore);

// CPU-side timeline semaphore operations
GPUKIT_API Res<> semaphore_signal(Semaphore semaphore, uint64_t value);
GPUKIT_API Res<> semaphore_wait(
		Semaphore semaphore, uint64_t value, uint64_t timeout_ns = UINT64_MAX);
GPUKIT_API Res<uint64_t> semaphore_get_value(Semaphore semaphore);

// =========================================================================
// Command Submission & Presentation
// =========================================================================

GPUKIT_API Res<> queue_submit(CommandQueue queue, CommandBuffer cmd, Fence fence = GL_NULL_HANDLE,
		Semaphore wait_semaphore = GL_NULL_HANDLE, Semaphore signal_semaphore = GL_NULL_HANDLE);
// Timeline-aware submit: value fields in SemaphoreSubmitInfo are used for timeline semaphores.
GPUKIT_API Res<> queue_submit(CommandQueue queue, CommandBuffer cmd,
		SemaphoreSubmitInfo wait_semaphore, SemaphoreSubmitInfo signal_semaphore,
		Fence fence = GL_NULL_HANDLE);
GPUKIT_API Res<> queue_present(
		CommandQueue queue, Swapchain swapchain, Semaphore wait_semaphore = GL_NULL_HANDLE);

// =========================================================================
// Command Recording
// =========================================================================

// Command Pool

GPUKIT_API Res<CommandPool> command_pool_create(CommandQueue queue);
GPUKIT_API Res<> command_pool_free(CommandPool command_pool);
GPUKIT_API Res<CommandBuffer> command_pool_allocate(CommandPool command_pool);
GPUKIT_API Res<std::vector<CommandBuffer>> command_pool_allocate(
		CommandPool command_pool, uint32_t count);
GPUKIT_API Res<> command_pool_reset(CommandPool command_pool);

// One-shot submission helper

GPUKIT_API Res<> command_immediate_submit(std::function<void(CommandBuffer cmd)>&& function,
		QueueType queue_type = QueueType::TRANSFER);

// Recording Control

GPUKIT_API Res<> command_begin(CommandBuffer cmd);
GPUKIT_API Res<> command_end(CommandBuffer cmd);
GPUKIT_API Res<> command_reset(CommandBuffer cmd);

// Render Passes

GPUKIT_API Res<> command_begin_render_pass(CommandBuffer cmd, RenderPass render_pass,
		FrameBuffer framebuffer, const Vec2u& draw_extent, Color clear_color = COLOR_GRAY);
GPUKIT_API Res<> command_end_render_pass(CommandBuffer cmd);

// Dynamic Rendering

GPUKIT_API Res<> command_begin_rendering(CommandBuffer cmd, const Vec2u& draw_extent,
		VectorView<RenderingAttachment> color_attachments, Image depth_attachment = GL_NULL_HANDLE);
GPUKIT_API Res<> command_end_rendering(CommandBuffer cmd);

// Pipeline & Binding

GPUKIT_API Res<> command_bind_graphics_pipeline(CommandBuffer cmd, Pipeline pipeline);
GPUKIT_API Res<> command_bind_compute_pipeline(CommandBuffer cmd, Pipeline pipeline);

GPUKIT_API Res<> command_bind_vertex_buffers(CommandBuffer cmd, uint32_t first_binding,
		VectorView<Buffer> vertex_buffers, VectorView<uint64_t> offsets);
GPUKIT_API Res<> command_bind_index_buffer(
		CommandBuffer cmd, Buffer index_buffer, uint64_t offset, IndexType index_type);

GPUKIT_API Res<> command_bind_uniform_sets(CommandBuffer cmd, Shader shader, uint32_t first_set,
		VectorView<UniformSet> uniform_sets, PipelineType type = PipelineType::GRAPHICS);
GPUKIT_API Res<> command_push_constants(CommandBuffer cmd, Shader shader, uint64_t offset,
		uint32_t size, const void* push_constants);

// Drawing

GPUKIT_API Res<> command_draw(CommandBuffer cmd, uint32_t vertex_count, uint32_t instance_count = 1,
		uint32_t first_vertex = 0, uint32_t first_instance = 0);
GPUKIT_API Res<> command_draw_indexed(CommandBuffer cmd, uint32_t index_count,
		uint32_t instance_count = 1, uint32_t first_index = 0, int32_t vertex_offset = 0,
		uint32_t first_instance = 0);
GPUKIT_API Res<> command_draw_indirect(
		CommandBuffer cmd, Buffer buffer, uint64_t offset, uint32_t draw_count, uint32_t stride);
GPUKIT_API Res<> command_draw_indexed_indirect(
		CommandBuffer cmd, Buffer buffer, uint64_t offset, uint32_t draw_count, uint32_t stride);
GPUKIT_API Res<> command_dispatch(
		CommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);
GPUKIT_API Res<> command_dispatch_indirect(CommandBuffer cmd, Buffer buffer, uint64_t offset);

// Operations & State

GPUKIT_API Res<> command_set_viewport(CommandBuffer cmd, const Vec2u& size);
GPUKIT_API Res<> command_set_scissor(
		CommandBuffer cmd, const Vec2u& size, const Vec2u& offset = { 0, 0 });
GPUKIT_API Res<> command_set_depth_bias(CommandBuffer cmd, float depth_bias_constant_factor,
		float depth_bias_clamp, float depth_bias_slope_factor);
GPUKIT_API Res<> command_clear_color(CommandBuffer cmd, Image image, const Color& clear_color,
		ImageAspectFlags image_aspect = IMAGE_ASPECT_COLOR_BIT);
GPUKIT_API Res<> command_clear_depth(
		CommandBuffer cmd, Image image, float depth = 1.0f, uint32_t stencil = 0);
GPUKIT_API Res<> command_reset_query_pool(
		CommandBuffer cmd, QueryPool pool, uint32_t first, uint32_t count);
GPUKIT_API Res<> command_write_timestamp(CommandBuffer cmd, QueryPool pool, uint32_t index,
		PipelineStageFlags stage = PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

// Copy / Barriers

GPUKIT_API Res<> command_copy_buffer(CommandBuffer cmd, Buffer src_buffer, Buffer dst_buffer,
		VectorView<BufferCopyRegion> regions);
GPUKIT_API Res<> command_buffer_memory_barrier(
		CommandBuffer cmd, BufferUsageFlags src_usage, BufferUsageFlags dst_usage, Buffer buffer);
GPUKIT_API Res<> command_pipeline_barrier(CommandBuffer cmd,
		VectorView<BufferBarrier> buffer_barriers, VectorView<ImageBarrier> image_barriers);
GPUKIT_API Res<> command_copy_buffer_to_image(CommandBuffer cmd, Buffer src_buffer, Image dst_image,
		VectorView<BufferImageCopyRegion> regions);
GPUKIT_API Res<> command_copy_image_to_image(CommandBuffer cmd, Image src_image, Image dst_image,
		const Vec2u& src_extent, const Vec2u& dst_extent, uint32_t src_mip_level = 0,
		uint32_t dst_mip_level = 0);
GPUKIT_API Res<> command_copy_image_to_buffer(CommandBuffer cmd, Image src_image, Buffer dst_buffer,
		VectorView<BufferImageCopyRegion> regions);
GPUKIT_API Res<> command_blit_image(CommandBuffer cmd, Image src_image, Image dst_image,
		const Vec2u& src_extent, const Vec2u& dst_extent, uint32_t src_mip_level = 0,
		uint32_t dst_mip_level = 0, ImageFiltering filter = ImageFiltering::LINEAR);
GPUKIT_API Res<> command_generate_mipmaps(CommandBuffer cmd, Image image);
GPUKIT_API Res<> command_transition_image(CommandBuffer cmd, Image image,
		ImageLayout current_layout, ImageLayout new_layout, uint32_t base_mip_level = 0,
		uint32_t level_count = GL_REMAINING_MIP_LEVELS);

// Utility

GPUKIT_API Res<> command_begin_label(CommandBuffer cmd, const char* name, Color color);
GPUKIT_API Res<> command_end_label(CommandBuffer cmd);
GPUKIT_API Res<> set_debug_name(ObjectType type, void* handle, const char* name);

} // namespace gpukit
