#include "gpukit/device.h"

#include "gpukit/version.h"
#include "platform/vulkan/vk_device.h"

namespace gpukit {

static VulkanDevice* g_device = nullptr;

// =========================================================================
// Lifecycle
// =========================================================================

Res<> init(const DeviceCreateInfo& info) {
	GPUKIT_LOG_INFO("[GPUKit] Version {}", GPUKIT_VERSION_STR);
	g_device = new VulkanDevice();
	if (auto r = g_device->init(info); !r) {
		delete g_device;
		g_device = nullptr;
		return r.error();
	}
	return {};
}

void shutdown() {
	delete g_device;
	g_device = nullptr;
}

// =========================================================================
// Device & Surface
// =========================================================================

Res<> device_wait() { return g_device->device_wait(); }
Res<> attach_surface(void* c, void* w) { return g_device->attach_surface(c, w); }
uint32_t get_max_msaa_samples() { return g_device->get_max_msaa_samples(); }
uint32_t get_max_bindless_instances() { return g_device->get_max_bindless_instances(); }
bool is_swapchain_supported() { return g_device->is_swapchain_supported(); }
NativeContext get_native_context() { return g_device->get_native_context(); }
Res<CommandQueue> queue_get(QueueType type) { return g_device->queue_get(type); }

// =========================================================================
// Swapchain
// =========================================================================

Res<Swapchain> swapchain_create() { return g_device->swapchain_create(); }

Res<> swapchain_resize(CommandQueue q, Swapchain sc, Vec2u size, bool vsync) {
	return g_device->swapchain_resize(q, sc, size, vsync);
}

Res<size_t> swapchain_get_image_count(Swapchain sc) {
	return g_device->swapchain_get_image_count(sc);
}

Res<std::vector<Image>> swapchain_get_images(Swapchain sc) {
	return g_device->swapchain_get_images(sc);
}

Res<Image> swapchain_acquire_image(Swapchain sc, Semaphore sem, uint32_t* o_idx) {
	return g_device->swapchain_acquire_image(sc, sem, o_idx);
}

Res<Vec2u> swapchain_get_extent(Swapchain sc) { return g_device->swapchain_get_extent(sc); }
Res<DataFormat> swapchain_get_format(Swapchain sc) { return g_device->swapchain_get_format(sc); }
Res<> swapchain_free(Swapchain sc) { return g_device->swapchain_free(sc); }

// =========================================================================
// Buffer
// =========================================================================

Res<Buffer> buffer_create(uint64_t size, BufferUsageFlags usage, MemoryAllocationType type) {
	return g_device->buffer_create(size, usage, type);
}

Res<> buffer_free(Buffer b) { return g_device->buffer_free(b); }

Res<BufferDeviceAddress> buffer_get_device_address(Buffer b) {
	return g_device->buffer_get_device_address(b);
}

Res<uint8_t*> buffer_map(Buffer b) { return g_device->buffer_map(b); }
Res<> buffer_unmap(Buffer b) { return g_device->buffer_unmap(b); }
Res<> buffer_invalidate(Buffer b) { return g_device->buffer_invalidate(b); }
Res<> buffer_flush(Buffer b) { return g_device->buffer_flush(b); }

// =========================================================================
// Image
// =========================================================================

Res<Image> image_create(const ImageCreateInfo& info) { return g_device->image_create(info); }
Res<> image_free(Image img) { return g_device->image_free(img); }
Res<Vec3u> image_get_size(Image img) { return g_device->image_get_size(img); }
Res<DataFormat> image_get_format(Image img) { return g_device->image_get_format(img); }
Res<uint32_t> image_get_mip_levels(Image img) { return g_device->image_get_mip_levels(img); }

Res<ImageUsageFlags> image_get_image_usage(Image img) {
	return g_device->image_get_image_usage(img);
}

Res<void*> image_get_native_view(Image img) { return g_device->image_get_native_view(img); }
Res<void*> sampler_get_native(Sampler s) { return g_device->sampler_get_native(s); }

// =========================================================================
// Sampler
// =========================================================================

Res<Sampler> sampler_create(const SamplerCreateInfo& info) {
	return g_device->sampler_create(info);
}

Res<> sampler_free(Sampler s) { return g_device->sampler_free(s); }

// =========================================================================
// Shader & Pipelines
// =========================================================================

Res<Shader> shader_create(VectorView<SpirvEntry> shaders) {
	return g_device->shader_create(shaders);
}

Res<Shader> shader_create(const char* vert, const char* frag) {
	return g_device->shader_create(vert, frag);
}

Res<Shader> shader_create(const char* compute) { return g_device->shader_create(compute); }
Res<> shader_free(Shader s) { return g_device->shader_free(s); }

Res<std::vector<ShaderInterfaceVariable>> shader_get_vertex_inputs(Shader s) {
	return g_device->shader_get_vertex_inputs(s);
}

Res<std::vector<ShaderResourceInfo>> shader_get_resources(Shader s) {
	return g_device->shader_get_resources(s);
}

Res<Pipeline> graphics_pipeline_create(const GraphicsPipelineCreateInfo& info) {
	return g_device->graphics_pipeline_create(info);
}

Res<Pipeline> compute_pipeline_create(Shader s) { return g_device->compute_pipeline_create(s); }
Res<> pipeline_free(Pipeline p) { return g_device->pipeline_free(p); }

// =========================================================================
// Uniform Sets
// =========================================================================

Res<UniformSet> uniform_set_create(
		VectorView<ShaderUniform> uniforms, Shader shader, uint32_t set_index) {
	return g_device->uniform_set_create(uniforms, shader, set_index);
}

Res<UniformSet> uniform_set_create(Shader shader, uint32_t set_index) {
	return g_device->uniform_set_create(shader, set_index);
}

Res<> uniform_set_free(UniformSet us) { return g_device->uniform_set_free(us); }

Res<UniformSet> uniform_set_create_bindless(
		Shader shader, uint32_t set_index, uint32_t binding_index, uint32_t max_count) {
	return g_device->uniform_set_create_bindless(shader, set_index, binding_index, max_count);
}

Res<> uniform_set_update_texture(
		UniformSet set, uint32_t binding, uint32_t array_index, Image image, Sampler sampler) {
	return g_device->uniform_set_update_texture(set, binding, array_index, image, sampler);
}

Res<> uniform_set_update_sampled_image(
		UniformSet set, uint32_t binding, uint32_t array_index, Image image) {
	return g_device->uniform_set_update_sampled_image(set, binding, array_index, image);
}

Res<> uniform_set_update_storage_image(
		UniformSet set, uint32_t binding, uint32_t array_index, Image image) {
	return g_device->uniform_set_update_storage_image(set, binding, array_index, image);
}

Res<> uniform_set_update_buffer(
		UniformSet set, uint32_t binding, uint32_t array_index, Buffer buffer) {
	return g_device->uniform_set_update_buffer(set, binding, array_index, buffer);
}

// =========================================================================
// Render Pass & Framebuffer
// =========================================================================

Res<RenderPass> render_pass_create(
		VectorView<RenderPassAttachment> attachments, VectorView<SubpassInfo> subpasses) {
	return g_device->render_pass_create(attachments, subpasses);
}

Res<> render_pass_destroy(RenderPass rp) { return g_device->render_pass_destroy(rp); }

Res<FrameBuffer> frame_buffer_create(
		RenderPass render_pass, VectorView<Image> attachments, const Vec2u& extent) {
	return g_device->frame_buffer_create(render_pass, attachments, extent);
}

Res<> frame_buffer_destroy(FrameBuffer fb) { return g_device->frame_buffer_destroy(fb); }

// =========================================================================
// Synchronization
// =========================================================================

Fence fence_create(bool create_signaled) { return g_device->fence_create(create_signaled); }
Res<> fence_free(Fence f) { return g_device->fence_free(f); }
Res<> fence_wait(Fence f) { return g_device->fence_wait(f); }
Res<> fence_reset(Fence f) { return g_device->fence_reset(f); }

Semaphore semaphore_create() { return g_device->semaphore_create(); }
Res<> semaphore_free(Semaphore s) { return g_device->semaphore_free(s); }

// =========================================================================
// Command Submission & Presentation
// =========================================================================

Res<> queue_submit(CommandQueue queue, CommandBuffer cmd, Fence fence, Semaphore wait_semaphore,
		Semaphore signal_semaphore) {
	return g_device->queue_submit(queue, cmd, fence, wait_semaphore, signal_semaphore);
}

Res<> queue_present(CommandQueue queue, Swapchain swapchain, Semaphore wait_semaphore) {
	return g_device->queue_present(queue, swapchain, wait_semaphore);
}

// =========================================================================
// Command Pool
// =========================================================================

Res<CommandPool> command_pool_create(CommandQueue queue) {
	return g_device->command_pool_create(queue);
}

Res<> command_pool_free(CommandPool cp) { return g_device->command_pool_free(cp); }

Res<CommandBuffer> command_pool_allocate(CommandPool cp) {
	return g_device->command_pool_allocate(cp);
}

Res<std::vector<CommandBuffer>> command_pool_allocate(CommandPool cp, uint32_t count) {
	return g_device->command_pool_allocate(cp, count);
}

Res<> command_pool_reset(CommandPool cp) { return g_device->command_pool_reset(cp); }

Res<> command_immediate_submit(
		std::function<void(CommandBuffer cmd)>&& function, QueueType queue_type) {
	return g_device->command_immediate_submit(std::move(function), queue_type);
}

// =========================================================================
// Recording Control
// =========================================================================

Res<> command_begin(CommandBuffer cmd) { return g_device->command_begin(cmd); }
Res<> command_end(CommandBuffer cmd) { return g_device->command_end(cmd); }
Res<> command_reset(CommandBuffer cmd) { return g_device->command_reset(cmd); }

// =========================================================================
// Render Passes
// =========================================================================

Res<> command_begin_render_pass(CommandBuffer cmd, RenderPass render_pass, FrameBuffer framebuffer,
		const Vec2u& draw_extent, Color clear_color) {
	return g_device->command_begin_render_pass(
			cmd, render_pass, framebuffer, draw_extent, clear_color);
}

Res<> command_end_render_pass(CommandBuffer cmd) { return g_device->command_end_render_pass(cmd); }

// =========================================================================
// Dynamic Rendering
// =========================================================================

Res<> command_begin_rendering(CommandBuffer cmd, const Vec2u& draw_extent,
		VectorView<RenderingAttachment> color_attachments, Image depth_attachment) {
	return g_device->command_begin_rendering(cmd, draw_extent, color_attachments, depth_attachment);
}

Res<> command_end_rendering(CommandBuffer cmd) { return g_device->command_end_rendering(cmd); }

// =========================================================================
// Pipeline & Binding
// =========================================================================

Res<> command_bind_graphics_pipeline(CommandBuffer cmd, Pipeline pipeline) {
	return g_device->command_bind_graphics_pipeline(cmd, pipeline);
}

Res<> command_bind_compute_pipeline(CommandBuffer cmd, Pipeline pipeline) {
	return g_device->command_bind_compute_pipeline(cmd, pipeline);
}

Res<> command_bind_vertex_buffers(CommandBuffer cmd, uint32_t first_binding,
		VectorView<Buffer> vertex_buffers, VectorView<uint64_t> offsets) {
	return g_device->command_bind_vertex_buffers(cmd, first_binding, vertex_buffers, offsets);
}

Res<> command_bind_index_buffer(
		CommandBuffer cmd, Buffer index_buffer, uint64_t offset, IndexType index_type) {
	return g_device->command_bind_index_buffer(cmd, index_buffer, offset, index_type);
}

Res<> command_bind_uniform_sets(CommandBuffer cmd, Shader shader, uint32_t first_set,
		VectorView<UniformSet> uniform_sets, PipelineType type) {
	return g_device->command_bind_uniform_sets(cmd, shader, first_set, uniform_sets, type);
}

Res<> command_push_constants(CommandBuffer cmd, Shader shader, uint64_t offset, uint32_t size,
		const void* push_constants) {
	return g_device->command_push_constants(cmd, shader, offset, size, push_constants);
}

// =========================================================================
// Drawing
// =========================================================================

Res<> command_draw(CommandBuffer cmd, uint32_t vertex_count, uint32_t instance_count,
		uint32_t first_vertex, uint32_t first_instance) {
	return g_device->command_draw(cmd, vertex_count, instance_count, first_vertex, first_instance);
}

Res<> command_draw_indexed(CommandBuffer cmd, uint32_t index_count, uint32_t instance_count,
		uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) {
	return g_device->command_draw_indexed(
			cmd, index_count, instance_count, first_index, vertex_offset, first_instance);
}

Res<> command_draw_indexed_indirect(
		CommandBuffer cmd, Buffer buffer, uint64_t offset, uint32_t draw_count, uint32_t stride) {
	return g_device->command_draw_indexed_indirect(cmd, buffer, offset, draw_count, stride);
}

Res<> command_dispatch(CommandBuffer cmd, uint32_t x, uint32_t y, uint32_t z) {
	return g_device->command_dispatch(cmd, x, y, z);
}

// =========================================================================
// Operations & State
// =========================================================================

Res<> command_set_viewport(CommandBuffer cmd, const Vec2u& size) {
	return g_device->command_set_viewport(cmd, size);
}

Res<> command_set_scissor(CommandBuffer cmd, const Vec2u& size, const Vec2u& offset) {
	return g_device->command_set_scissor(cmd, size, offset);
}

Res<> command_set_depth_bias(
		CommandBuffer cmd, float constant_factor, float clamp, float slope_factor) {
	return g_device->command_set_depth_bias(cmd, constant_factor, clamp, slope_factor);
}

Res<> command_clear_color(
		CommandBuffer cmd, Image image, const Color& clear_color, ImageAspectFlags image_aspect) {
	return g_device->command_clear_color(cmd, image, clear_color, image_aspect);
}

// =========================================================================
// Copy / Barriers
// =========================================================================

Res<> command_copy_buffer(CommandBuffer cmd, Buffer src_buffer, Buffer dst_buffer,
		VectorView<BufferCopyRegion> regions) {
	return g_device->command_copy_buffer(cmd, src_buffer, dst_buffer, regions);
}

Res<> command_buffer_memory_barrier(
		CommandBuffer cmd, BufferUsageFlags src_usage, BufferUsageFlags dst_usage, Buffer buffer) {
	return g_device->command_buffer_memory_barrier(cmd, src_usage, dst_usage, buffer);
}

Res<> command_pipeline_barrier(CommandBuffer cmd, VectorView<BufferBarrier> buffer_barriers,
		VectorView<ImageBarrier> image_barriers) {
	return g_device->command_pipeline_barrier(cmd, buffer_barriers, image_barriers);
}

Res<> command_copy_buffer_to_image(CommandBuffer cmd, Buffer src_buffer, Image dst_image,
		VectorView<BufferImageCopyRegion> regions) {
	return g_device->command_copy_buffer_to_image(cmd, src_buffer, dst_image, regions);
}

Res<> command_copy_image_to_image(CommandBuffer cmd, Image src_image, Image dst_image,
		const Vec2u& src_extent, const Vec2u& dst_extent, uint32_t src_mip_level,
		uint32_t dst_mip_level) {
	return g_device->command_copy_image_to_image(
			cmd, src_image, dst_image, src_extent, dst_extent, src_mip_level, dst_mip_level);
}

Res<> command_transition_image(CommandBuffer cmd, Image image, ImageLayout current_layout,
		ImageLayout new_layout, uint32_t base_mip_level, uint32_t level_count) {
	return g_device->command_transition_image(
			cmd, image, current_layout, new_layout, base_mip_level, level_count);
}

// =========================================================================
// Utility
// =========================================================================

Res<> command_begin_label(CommandBuffer cmd, const char* name, Color color) {
	return g_device->command_begin_label(cmd, name, color);
}

Res<> command_end_label(CommandBuffer cmd) { return g_device->command_end_label(cmd); }

Res<> set_debug_name(ObjectType type, void* handle, const char* name) {
	return g_device->set_debug_name(type, handle, name);
}

} // namespace gpukit
