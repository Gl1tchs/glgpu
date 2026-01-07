#pragma once

#include "glgpu/color.h"
#include "glgpu/types.h"
#include "glgpu/vector_view.h"

#include <functional>
#include <memory>

namespace gl {

enum DeviceRequiredFeatureBits : uint32_t {
	RENDER_BACKEND_FEATURE_NONE = 0x0,
	RENDER_BACKEND_FEATURE_SWAPCHAIN_BIT = 0x1,
	RENDER_BACKEND_FEATURE_ENSURE_SURFACE_SUPPORT = 0x2,
	RENDER_BACKEND_FEATURE_DISTINCT_COMPUTE_QUEUE_BIT = 0x4,
};
typedef uint32_t DeviceFeatureFlags;

struct DeviceCreateInfo {
	RenderAPI api = RenderAPI::VULKAN;
	DeviceFeatureFlags required_features = RENDER_BACKEND_FEATURE_NONE;
	void* native_connection_handle = nullptr; // Windows HINSTANCE or X11 Display
	void* native_window_handle = nullptr; // HWND or XWindow
};

/**
 * Abstract class responsible for communicating with the GPU.
 */
class Device {
public:
	virtual ~Device() = default;

	static Res<std::shared_ptr<Device>> create(const DeviceCreateInfo& info);

	virtual Res<> init(const DeviceCreateInfo& info) = 0;

	// =========================================================================
	// Device & Surface
	// =========================================================================

	virtual Res<> device_wait() = 0;

	virtual Res<> attach_surface(void* connection_handle, void* window_handle) = 0;

	virtual uint32_t get_max_msaa_samples() const = 0;

	virtual uint32_t get_max_bindless_instances() const = 0;

	virtual bool is_swapchain_supported() = 0;

	virtual Res<CommandQueue> queue_get(QueueType type) = 0;

	// =========================================================================
	// Swapchain
	// =========================================================================

	virtual Res<Swapchain> swapchain_create() = 0;

	virtual Res<> swapchain_resize(
			CommandQueue cmd_queue, Swapchain swapchain, Vec2u size, bool vsync = false) = 0;

	virtual Res<size_t> swapchain_get_image_count(Swapchain swapchain) = 0;
	virtual Res<std::vector<Image>> swapchain_get_images(Swapchain swapchain) = 0;

	// Returns Image or Error if resize is needed

	virtual Res<Image> swapchain_acquire_image(
			Swapchain swapchain, Semaphore semaphore, uint32_t* o_image_index = nullptr) = 0;

	virtual Res<Vec2u> swapchain_get_extent(Swapchain swapchain) = 0;
	virtual Res<DataFormat> swapchain_get_format(Swapchain swapchain) = 0;
	virtual Res<> swapchain_free(Swapchain swapchain) = 0;

	// =========================================================================
	// Resource Management (Buffers & Images)
	// =========================================================================

	// Buffer

	virtual Res<Buffer> buffer_create(
			uint64_t size, BufferUsageFlags usage, MemoryAllocationType allocation_type) = 0;
	virtual Res<> buffer_free(Buffer buffer) = 0;
	virtual Res<BufferDeviceAddress> buffer_get_device_address(Buffer buffer) = 0;

	virtual Res<uint8_t*> buffer_map(Buffer buffer) = 0;
	virtual Res<> buffer_unmap(Buffer buffer) = 0;
	virtual Res<> buffer_invalidate(Buffer buffer) = 0; // GPU -> CPU
	virtual Res<> buffer_flush(Buffer buffer) = 0; // CPU -> GPU

	// Image

	virtual Res<Image> image_create(const ImageCreateInfo& info) = 0;
	virtual Res<> image_free(Image image) = 0;
	virtual Res<Vec3u> image_get_size(Image image) = 0;
	virtual Res<DataFormat> image_get_format(Image image) = 0;
	virtual Res<uint32_t> image_get_mip_levels(Image image) = 0;
	virtual Res<ImageUsageFlags> image_get_image_usage(Image image) = 0;

	// Sampler

	virtual Res<Sampler> sampler_create(const SamplerCreateInfo& info) = 0;
	virtual Res<> sampler_free(Sampler sampler) = 0;

	// =========================================================================
	// Shader & Pipelines
	// =========================================================================

	virtual Res<Shader> shader_create_from_bytecode(VectorView<SpirvEntry> shaders) = 0;
	virtual Res<> shader_free(Shader shader) = 0;
	virtual Res<std::vector<ShaderInterfaceVariable>> shader_get_vertex_inputs(Shader shader) = 0;

	// Pipeline Creation using the new consolidated struct

	virtual Res<Pipeline> graphics_pipeline_create(const GraphicsPipelineCreateInfo& info) = 0;
	virtual Res<Pipeline> compute_pipeline_create(Shader shader) = 0;
	virtual Res<> pipeline_free(Pipeline pipeline) = 0;

	// Descriptors / Uniforms

	virtual Res<UniformSet> uniform_set_create(
			VectorView<ShaderUniform> uniforms, Shader shader, uint32_t set_index) = 0;
	virtual Res<> uniform_set_free(UniformSet uniform_set) = 0;

	virtual Res<UniformSet> uniform_set_create_bindless(
			Shader shader, uint32_t set_index, uint32_t binding_index, uint32_t max_count) = 0;

	/**
	 * Update bindless uniform set texture at given index
	 *
	 * NOTE: Since SPIRV does not support reflection on non uniform types,
	 * user must add 'h_' prefix to the uniform for engine to recognize
	 * the binding as 'bindless'
	 */
	virtual Res<> uniform_set_update_texture(UniformSet set, uint32_t binding, uint32_t array_index,
			Image image, Sampler sampler) = 0;

	// =========================================================================
	// Render Pass & Framebuffer (Legacy)
	// =========================================================================

	virtual Res<RenderPass> render_pass_create(
			VectorView<RenderPassAttachment> attachments, VectorView<SubpassInfo> subpasses) = 0;
	virtual Res<> render_pass_destroy(RenderPass render_pass) = 0;

	virtual Res<FrameBuffer> frame_buffer_create(
			RenderPass render_pass, VectorView<Image> attachments, const Vec2u& extent) = 0;
	virtual Res<> frame_buffer_destroy(FrameBuffer frame_buffer) = 0;

	// =========================================================================
	// Synchronization
	// =========================================================================

	virtual Fence fence_create(bool create_signaled = true) = 0;
	virtual Res<> fence_free(Fence fence) = 0;
	virtual Res<> fence_wait(Fence fence) = 0;
	virtual Res<> fence_reset(Fence fence) = 0;

	virtual Semaphore semaphore_create() = 0;
	virtual Res<> semaphore_free(Semaphore semaphore) = 0;

	// =========================================================================
	// Command Submission & Presentation
	// =========================================================================

	virtual Res<> queue_submit(CommandQueue queue, CommandBuffer cmd, Fence fence = GL_NULL_HANDLE,
			Semaphore wait_semaphore = GL_NULL_HANDLE,
			Semaphore signal_semaphore = GL_NULL_HANDLE) = 0;

	// Returns true if success, false if resize needed

	virtual Res<> queue_present(
			CommandQueue queue, Swapchain swapchain, Semaphore wait_semaphore = GL_NULL_HANDLE) = 0;

	// =========================================================================
	// Command Recording
	// =========================================================================

	// Command Pool

	virtual Res<CommandPool> command_pool_create(CommandQueue queue) = 0;
	virtual Res<> command_pool_free(CommandPool command_pool) = 0;
	virtual Res<CommandBuffer> command_pool_allocate(CommandPool command_pool) = 0;
	virtual Res<std::vector<CommandBuffer>> command_pool_allocate(
			CommandPool command_pool, const uint32_t count) = 0;
	virtual Res<> command_pool_reset(CommandPool command_pool) = 0;

	// One-shot submission helper

	virtual Res<> command_immediate_submit(std::function<void(CommandBuffer cmd)>&& function,
			QueueType queue_type = QueueType::TRANSFER) = 0;

	// Recording Control

	virtual Res<> command_begin(CommandBuffer cmd) = 0;
	virtual Res<> command_end(CommandBuffer cmd) = 0;
	virtual Res<> command_reset(CommandBuffer cmd) = 0;

	// Render Passes

	virtual Res<> command_begin_render_pass(CommandBuffer cmd, RenderPass render_pass,
			FrameBuffer framebuffer, const Vec2u& draw_extent, Color clear_color = COLOR_GRAY) = 0;
	virtual Res<> command_end_render_pass(CommandBuffer cmd) = 0;

	// Dynamic Rendering

	virtual Res<> command_begin_rendering(CommandBuffer cmd, const Vec2u& draw_extent,
			VectorView<RenderingAttachment> color_attachments,
			Image depth_attachment = GL_NULL_HANDLE) = 0;
	virtual Res<> command_end_rendering(CommandBuffer cmd) = 0;

	// Pipeline & Binding

	virtual Res<> command_bind_graphics_pipeline(CommandBuffer cmd, Pipeline pipeline) = 0;
	virtual Res<> command_bind_compute_pipeline(CommandBuffer cmd, Pipeline pipeline) = 0;

	virtual Res<> command_bind_vertex_buffers(CommandBuffer cmd, uint32_t first_binding,
			VectorView<Buffer> vertex_buffers, VectorView<uint64_t> offsets) = 0;
	virtual Res<> command_bind_index_buffer(
			CommandBuffer cmd, Buffer index_buffer, uint64_t offset, IndexType index_type) = 0;

	virtual Res<> command_bind_uniform_sets(CommandBuffer cmd, Shader shader, uint32_t first_set,
			VectorView<UniformSet> uniform_sets, PipelineType type = PipelineType::GRAPHICS) = 0;
	virtual Res<> command_push_constants(CommandBuffer cmd, Shader shader, uint64_t offset,
			uint32_t size, const void* push_constants) = 0;

	// Drawing

	virtual Res<> command_draw(CommandBuffer cmd, uint32_t vertex_count,
			uint32_t instance_count = 1, uint32_t first_vertex = 0,
			uint32_t first_instance = 0) = 0;

	virtual Res<> command_draw_indexed(CommandBuffer cmd, uint32_t index_count,
			uint32_t instance_count = 1, uint32_t first_index = 0, int32_t vertex_offset = 0,
			uint32_t first_instance = 0) = 0;

	virtual Res<> command_draw_indexed_indirect(CommandBuffer cmd, Buffer buffer, uint64_t offset,
			uint32_t draw_count, uint32_t stride) = 0;

	virtual Res<> command_dispatch(CommandBuffer cmd, uint32_t group_count_x,
			uint32_t group_count_y, uint32_t group_count_z) = 0;

	// Operations & State

	virtual Res<> command_set_viewport(CommandBuffer cmd, const Vec2u& size) = 0;
	virtual Res<> command_set_scissor(
			CommandBuffer cmd, const Vec2u& size, const Vec2u& offset = { 0, 0 }) = 0;
	virtual Res<> command_set_depth_bias(CommandBuffer cmd, float depth_bias_constant_factor,
			float depth_bias_clamp, float depth_bias_slope_factor) = 0;

	virtual Res<> command_clear_color(CommandBuffer cmd, Image image, const Color& clear_color,
			ImageAspectFlags image_aspect = IMAGE_ASPECT_COLOR_BIT) = 0;

	// Copy / Barriers

	virtual Res<> command_copy_buffer(CommandBuffer cmd, Buffer src_buffer, Buffer dst_buffer,
			VectorView<BufferCopyRegion> regions) = 0;

	virtual Res<> command_buffer_memory_barrier(CommandBuffer cmd, BufferUsageFlags src_usage,
			BufferUsageFlags dst_usage, Buffer buffer) = 0;

	virtual Res<> command_copy_buffer_to_image(CommandBuffer cmd, Buffer src_buffer,
			Image dst_image, VectorView<BufferImageCopyRegion> regions) = 0;

	virtual Res<> command_copy_image_to_image(CommandBuffer cmd, Image src_image, Image dst_image,
			const Vec2u& src_extent, const Vec2u& dst_extent, uint32_t src_mip_level = 0,
			uint32_t dst_mip_level = 0) = 0;

	virtual Res<> command_transition_image(CommandBuffer cmd, Image image,
			ImageLayout current_layout, ImageLayout new_layout, uint32_t base_mip_level = 0,
			uint32_t level_count = GL_REMAINING_MIP_LEVELS) = 0;

	// Utility functions

	virtual Res<> command_begin_label(CommandBuffer cmd, const char* name, Color color) = 0;
	virtual Res<> command_end_label(CommandBuffer cmd) = 0;
};

} // namespace gl
