#include "gpukit/device.h"

#include "gpukit/version.h"
#include "platform/vulkan/vk_device.h"

namespace gpukit {

thread_local VulkanDevice* t_current = nullptr;

static VulkanDevice& current() {
	GPUKIT_ASSERT(t_current != nullptr, "No device selected. Call gpukit::init() first.");
	return *t_current;
}

// =========================================================================
// Lifecycle
// =========================================================================

Res<DeviceHandle> init(const DeviceCreateInfo& info) {
	GPUKIT_LOG_INFO("[GPUKit] Version {}", GPUKIT_VERSION_STR);
	VulkanDevice* dev = new VulkanDevice();
	if (auto r = dev->init(info); !r) {
		delete dev;
		return make_err<DeviceHandle>(r.error());
	}
	t_current = dev;
	return reinterpret_cast<DeviceHandle>(dev);
}

void shutdown(DeviceHandle device) {
	VulkanDevice* dev = reinterpret_cast<VulkanDevice*>(device);
	if (t_current == dev)
		t_current = nullptr;
	delete dev;
}

void shutdown() {
	if (!t_current) {
		GPUKIT_LOG_ERROR("Unable to shutdown current device");
		return;
	}

	delete t_current;
	t_current = nullptr;
}

void select_device(DeviceHandle device) { t_current = reinterpret_cast<VulkanDevice*>(device); }

// =========================================================================
// Device & Surface
// =========================================================================

Res<> device_wait() { return current().device_wait(); }
Res<> attach_surface(void* c, void* w) { return current().attach_surface(c, w); }
uint32_t get_max_msaa_samples() { return current().get_max_msaa_samples(); }
uint32_t get_max_bindless_instances() { return current().get_max_bindless_instances(); }
bool is_swapchain_supported() { return current().is_swapchain_supported(); }
NativeContext get_native_context() { return current().get_native_context(); }
Res<CommandQueue> queue_get(QueueType type) { return current().queue_get(type); }

// =========================================================================
// Swapchain
// =========================================================================

Res<Swapchain> swapchain_create() { return current().swapchain_create(); }

Res<> swapchain_resize(CommandQueue q, Swapchain sc, Vec2u size, bool vsync) {
	return current().swapchain_resize(q, sc, size, vsync);
}

Res<size_t> swapchain_get_image_count(Swapchain sc) {
	return current().swapchain_get_image_count(sc);
}

Res<std::vector<Image>> swapchain_get_images(Swapchain sc) {
	return current().swapchain_get_images(sc);
}

Res<Image> swapchain_acquire_image(Swapchain sc, Semaphore sem, uint32_t* o_idx) {
	return current().swapchain_acquire_image(sc, sem, o_idx);
}

Res<Vec2u> swapchain_get_extent(Swapchain sc) { return current().swapchain_get_extent(sc); }
Res<DataFormat> swapchain_get_format(Swapchain sc) { return current().swapchain_get_format(sc); }
Res<uint32_t> swapchain_get_pre_transform(Swapchain sc) {
	return current().swapchain_get_pre_transform(sc);
}
Res<> swapchain_free(Swapchain sc) { return current().swapchain_free(sc); }

// =========================================================================
// Buffer
// =========================================================================

Res<Buffer> buffer_create(uint64_t size, BufferUsageFlags usage, MemoryAllocationType type) {
	return current().buffer_create(size, usage, type);
}

Res<> buffer_free(Buffer b) { return current().buffer_free(b); }

Res<BufferDeviceAddress> buffer_get_device_address(Buffer b) {
	return current().buffer_get_device_address(b);
}

Res<uint8_t*> buffer_map(Buffer b) { return current().buffer_map(b); }
Res<> buffer_unmap(Buffer b) { return current().buffer_unmap(b); }
Res<> buffer_invalidate(Buffer b) { return current().buffer_invalidate(b); }
Res<> buffer_flush(Buffer b) { return current().buffer_flush(b); }

Res<> buffer_upload(Buffer b, const void* data, size_t size, size_t offset) {
	return current().buffer_upload(b, data, size, offset);
}

// =========================================================================
// Image
// =========================================================================

Res<Image> image_create(const ImageCreateInfo& info) { return current().image_create(info); }
Res<> image_free(Image img) { return current().image_free(img); }

Res<> image_upload(Image img, const void* data, size_t size) {
	return current().image_upload(img, data, size);
}
Res<Vec3u> image_get_size(Image img) { return current().image_get_size(img); }
Res<DataFormat> image_get_format(Image img) { return current().image_get_format(img); }
Res<uint32_t> image_get_mip_levels(Image img) { return current().image_get_mip_levels(img); }

Res<ImageUsageFlags> image_get_image_usage(Image img) {
	return current().image_get_image_usage(img);
}

Res<void*> image_get_native_view(Image img) { return current().image_get_native_view(img); }
Res<void*> sampler_get_native(Sampler s) { return current().sampler_get_native(s); }

// =========================================================================
// Sampler
// =========================================================================

Res<Sampler> sampler_create(const SamplerCreateInfo& info) {
	return current().sampler_create(info);
}

Res<> sampler_free(Sampler s) { return current().sampler_free(s); }

// =========================================================================
// Shader & Pipelines
// =========================================================================

Res<Shader> shader_create(VectorView<SpirvEntry> shaders) {
	return current().shader_create(shaders);
}

Res<Shader> shader_create(const char* vert, const char* frag) {
	return current().shader_create(vert, frag);
}

Res<Shader> shader_create(const char* compute) { return current().shader_create(compute); }
Res<> shader_free(Shader s) { return current().shader_free(s); }

Res<std::vector<ShaderInterfaceVariable>> shader_get_vertex_inputs(Shader s) {
	return current().shader_get_vertex_inputs(s);
}

Res<std::vector<ShaderResourceInfo>> shader_get_resources(Shader s) {
	return current().shader_get_resources(s);
}

Res<Pipeline> graphics_pipeline_create(const GraphicsPipelineCreateInfo& info) {
	return current().graphics_pipeline_create(info);
}

Res<Pipeline> compute_pipeline_create(Shader s) { return current().compute_pipeline_create(s); }

Res<Pipeline> compute_pipeline_create(Shader s, VectorView<SpecializationConstant> c) {
	return current().compute_pipeline_create(s, c);
}

Res<> pipeline_free(Pipeline p) { return current().pipeline_free(p); }

// =========================================================================
// Uniform Sets
// =========================================================================

Res<UniformSet> uniform_set_create(
		VectorView<ShaderUniform> uniforms, Shader shader, uint32_t set_index) {
	return current().uniform_set_create(uniforms, shader, set_index);
}

Res<UniformSet> uniform_set_create(Shader shader, uint32_t set_index) {
	return current().uniform_set_create(shader, set_index);
}

Res<> uniform_set_free(UniformSet us) { return current().uniform_set_free(us); }

Res<UniformSet> uniform_set_create_bindless(
		Shader shader, uint32_t set_index, uint32_t binding_index, uint32_t max_count) {
	return current().uniform_set_create_bindless(shader, set_index, binding_index, max_count);
}

Res<> uniform_set_update_texture(
		UniformSet set, uint32_t binding, uint32_t array_index, Image image, Sampler sampler) {
	return current().uniform_set_update_texture(set, binding, array_index, image, sampler);
}

Res<> uniform_set_update_sampled_image(
		UniformSet set, uint32_t binding, uint32_t array_index, Image image) {
	return current().uniform_set_update_sampled_image(set, binding, array_index, image);
}

Res<> uniform_set_update_storage_image(
		UniformSet set, uint32_t binding, uint32_t array_index, Image image) {
	return current().uniform_set_update_storage_image(set, binding, array_index, image);
}

Res<> uniform_set_update_buffer(
		UniformSet set, uint32_t binding, uint32_t array_index, Buffer buffer) {
	return current().uniform_set_update_buffer(set, binding, array_index, buffer);
}

Res<> uniform_set_update_buffer_range(UniformSet set, uint32_t binding, uint32_t array_index,
		Buffer buffer, uint64_t offset, uint64_t range) {
	return current().uniform_set_update_buffer_range(
			set, binding, array_index, buffer, offset, range);
}

// =========================================================================
// Render Pass & Framebuffer
// =========================================================================

Res<RenderPass> render_pass_create(
		VectorView<RenderPassAttachment> attachments, VectorView<SubpassInfo> subpasses) {
	return current().render_pass_create(attachments, subpasses);
}

Res<> render_pass_destroy(RenderPass rp) { return current().render_pass_destroy(rp); }

Res<FrameBuffer> frame_buffer_create(
		RenderPass render_pass, VectorView<Image> attachments, const Vec2u& extent) {
	return current().frame_buffer_create(render_pass, attachments, extent);
}

Res<> frame_buffer_destroy(FrameBuffer fb) { return current().frame_buffer_destroy(fb); }

// =========================================================================
// Query Pools
// =========================================================================

Res<QueryPool> query_pool_create(uint32_t count) { return current().query_pool_create(count); }

Res<> query_pool_free(QueryPool pool) { return current().query_pool_free(pool); }

Res<std::vector<uint64_t>> query_pool_get_results(QueryPool pool, uint32_t first, uint32_t count) {
	return current().query_pool_get_results(pool, first, count);
}

double timestamps_to_ns(uint64_t ticks) { return current().timestamps_to_ns(ticks); }

// =========================================================================
// Synchronization
// =========================================================================

Fence fence_create(bool create_signaled) { return current().fence_create(create_signaled); }
Res<> fence_free(Fence f) { return current().fence_free(f); }
Res<> fence_wait(Fence f) { return current().fence_wait(f); }
Res<> fence_reset(Fence f) { return current().fence_reset(f); }

Semaphore semaphore_create() { return current().semaphore_create(); }

Res<Semaphore> timeline_semaphore_create(uint64_t initial_value) {
	return current().timeline_semaphore_create(initial_value);
}

Res<> semaphore_free(Semaphore s) { return current().semaphore_free(s); }

Res<> semaphore_signal(Semaphore s, uint64_t value) { return current().semaphore_signal(s, value); }

Res<> semaphore_wait(Semaphore s, uint64_t value, uint64_t timeout_ns) {
	return current().semaphore_wait(s, value, timeout_ns);
}

Res<uint64_t> semaphore_get_value(Semaphore s) { return current().semaphore_get_value(s); }

// =========================================================================
// Command Submission & Presentation
// =========================================================================

Res<> queue_submit(CommandQueue queue, CommandBuffer cmd, Fence fence, Semaphore wait_semaphore,
		Semaphore signal_semaphore) {
	return current().queue_submit(queue, cmd, fence, wait_semaphore, signal_semaphore);
}
Res<> queue_submit(CommandQueue queue, CommandBuffer cmd, SemaphoreSubmitInfo wait,
		SemaphoreSubmitInfo signal, Fence fence) {
	return current().queue_submit(queue, cmd, wait, signal, fence);
}

Res<> queue_present(CommandQueue queue, Swapchain swapchain, Semaphore wait_semaphore) {
	return current().queue_present(queue, swapchain, wait_semaphore);
}

// =========================================================================
// Command Pool
// =========================================================================

Res<CommandPool> command_pool_create(CommandQueue queue) {
	return current().command_pool_create(queue);
}

Res<> command_pool_free(CommandPool cp) { return current().command_pool_free(cp); }

Res<CommandBuffer> command_pool_allocate(CommandPool cp) {
	return current().command_pool_allocate(cp);
}

Res<std::vector<CommandBuffer>> command_pool_allocate(CommandPool cp, uint32_t count) {
	return current().command_pool_allocate(cp, count);
}

Res<> command_pool_reset(CommandPool cp) { return current().command_pool_reset(cp); }

Res<> command_immediate_submit(
		std::function<void(CommandBuffer cmd)>&& function, QueueType queue_type) {
	return current().command_immediate_submit(std::move(function), queue_type);
}

// =========================================================================
// Recording Control
// =========================================================================

Res<> command_begin(CommandBuffer cmd) { return current().command_begin(cmd); }
Res<> command_end(CommandBuffer cmd) { return current().command_end(cmd); }
Res<> command_reset(CommandBuffer cmd) { return current().command_reset(cmd); }

// =========================================================================
// Render Passes
// =========================================================================

Res<> command_begin_render_pass(CommandBuffer cmd, RenderPass render_pass, FrameBuffer framebuffer,
		const Vec2u& draw_extent, Color clear_color) {
	return current().command_begin_render_pass(
			cmd, render_pass, framebuffer, draw_extent, clear_color);
}

Res<> command_end_render_pass(CommandBuffer cmd) { return current().command_end_render_pass(cmd); }

// =========================================================================
// Dynamic Rendering
// =========================================================================

Res<> command_begin_rendering(CommandBuffer cmd, const Vec2u& draw_extent,
		VectorView<RenderingAttachment> color_attachments, Image depth_attachment) {
	return current().command_begin_rendering(cmd, draw_extent, color_attachments, depth_attachment);
}

Res<> command_end_rendering(CommandBuffer cmd) { return current().command_end_rendering(cmd); }

// =========================================================================
// Pipeline & Binding
// =========================================================================

Res<> command_bind_graphics_pipeline(CommandBuffer cmd, Pipeline pipeline) {
	return current().command_bind_graphics_pipeline(cmd, pipeline);
}

Res<> command_bind_compute_pipeline(CommandBuffer cmd, Pipeline pipeline) {
	return current().command_bind_compute_pipeline(cmd, pipeline);
}

Res<> command_bind_vertex_buffers(CommandBuffer cmd, uint32_t first_binding,
		VectorView<Buffer> vertex_buffers, VectorView<uint64_t> offsets) {
	return current().command_bind_vertex_buffers(cmd, first_binding, vertex_buffers, offsets);
}

Res<> command_bind_index_buffer(
		CommandBuffer cmd, Buffer index_buffer, uint64_t offset, IndexType index_type) {
	return current().command_bind_index_buffer(cmd, index_buffer, offset, index_type);
}

Res<> command_bind_uniform_sets(CommandBuffer cmd, Shader shader, uint32_t first_set,
		VectorView<UniformSet> uniform_sets, PipelineType type) {
	return current().command_bind_uniform_sets(cmd, shader, first_set, uniform_sets, type);
}

Res<> command_push_constants(CommandBuffer cmd, Shader shader, uint64_t offset, uint32_t size,
		const void* push_constants) {
	return current().command_push_constants(cmd, shader, offset, size, push_constants);
}

// =========================================================================
// Drawing
// =========================================================================

Res<> command_draw(CommandBuffer cmd, uint32_t vertex_count, uint32_t instance_count,
		uint32_t first_vertex, uint32_t first_instance) {
	return current().command_draw(cmd, vertex_count, instance_count, first_vertex, first_instance);
}

Res<> command_draw_indexed(CommandBuffer cmd, uint32_t index_count, uint32_t instance_count,
		uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) {
	return current().command_draw_indexed(
			cmd, index_count, instance_count, first_index, vertex_offset, first_instance);
}

Res<> command_draw_indirect(
		CommandBuffer cmd, Buffer buffer, uint64_t offset, uint32_t draw_count, uint32_t stride) {
	return current().command_draw_indirect(cmd, buffer, offset, draw_count, stride);
}

Res<> command_draw_indexed_indirect(
		CommandBuffer cmd, Buffer buffer, uint64_t offset, uint32_t draw_count, uint32_t stride) {
	return current().command_draw_indexed_indirect(cmd, buffer, offset, draw_count, stride);
}

Res<> command_dispatch(CommandBuffer cmd, uint32_t x, uint32_t y, uint32_t z) {
	return current().command_dispatch(cmd, x, y, z);
}

Res<> command_dispatch_indirect(CommandBuffer cmd, Buffer buffer, uint64_t offset) {
	return current().command_dispatch_indirect(cmd, buffer, offset);
}

// =========================================================================
// Operations & State
// =========================================================================

Res<> command_set_viewport(CommandBuffer cmd, const Vec2u& size) {
	return current().command_set_viewport(cmd, size);
}

Res<> command_set_scissor(CommandBuffer cmd, const Vec2u& size, const Vec2u& offset) {
	return current().command_set_scissor(cmd, size, offset);
}

Res<> command_set_depth_bias(
		CommandBuffer cmd, float constant_factor, float clamp, float slope_factor) {
	return current().command_set_depth_bias(cmd, constant_factor, clamp, slope_factor);
}

Res<> command_clear_color(
		CommandBuffer cmd, Image image, const Color& clear_color, ImageAspectFlags image_aspect) {
	return current().command_clear_color(cmd, image, clear_color, image_aspect);
}

Res<> command_clear_depth(CommandBuffer cmd, Image image, float depth, uint32_t stencil) {
	return current().command_clear_depth(cmd, image, depth, stencil);
}

Res<> command_reset_query_pool(CommandBuffer cmd, QueryPool pool, uint32_t first, uint32_t count) {
	return current().command_reset_query_pool(cmd, pool, first, count);
}

Res<> command_write_timestamp(
		CommandBuffer cmd, QueryPool pool, uint32_t index, PipelineStageFlags stage) {
	return current().command_write_timestamp(cmd, pool, index, stage);
}

// =========================================================================
// Copy / Barriers
// =========================================================================

Res<> command_copy_buffer(CommandBuffer cmd, Buffer src_buffer, Buffer dst_buffer,
		VectorView<BufferCopyRegion> regions) {
	return current().command_copy_buffer(cmd, src_buffer, dst_buffer, regions);
}

Res<> command_buffer_memory_barrier(
		CommandBuffer cmd, BufferUsageFlags src_usage, BufferUsageFlags dst_usage, Buffer buffer) {
	return current().command_buffer_memory_barrier(cmd, src_usage, dst_usage, buffer);
}

Res<> command_pipeline_barrier(CommandBuffer cmd, VectorView<BufferBarrier> buffer_barriers,
		VectorView<ImageBarrier> image_barriers) {
	return current().command_pipeline_barrier(cmd, buffer_barriers, image_barriers);
}

Res<> command_copy_buffer_to_image(CommandBuffer cmd, Buffer src_buffer, Image dst_image,
		VectorView<BufferImageCopyRegion> regions) {
	return current().command_copy_buffer_to_image(cmd, src_buffer, dst_image, regions);
}

Res<> command_copy_image_to_image(CommandBuffer cmd, Image src_image, Image dst_image,
		const Vec2u& src_extent, const Vec2u& dst_extent, uint32_t src_mip_level,
		uint32_t dst_mip_level) {
	return current().command_copy_image_to_image(
			cmd, src_image, dst_image, src_extent, dst_extent, src_mip_level, dst_mip_level);
}

Res<> command_copy_image_to_buffer(CommandBuffer cmd, Image src_image, Buffer dst_buffer,
		VectorView<BufferImageCopyRegion> regions) {
	return current().command_copy_image_to_buffer(cmd, src_image, dst_buffer, regions);
}

Res<> command_blit_image(CommandBuffer cmd, Image src_image, Image dst_image,
		const Vec2u& src_extent, const Vec2u& dst_extent, uint32_t src_mip_level,
		uint32_t dst_mip_level, ImageFiltering filter) {
	return current().command_blit_image(cmd, src_image, dst_image, src_extent, dst_extent,
			src_mip_level, dst_mip_level, filter);
}

Res<> command_generate_mipmaps(CommandBuffer cmd, Image image) {
	return current().command_generate_mipmaps(cmd, image);
}

Res<> command_transition_image(CommandBuffer cmd, Image image, ImageLayout current_layout,
		ImageLayout new_layout, uint32_t base_mip_level, uint32_t level_count) {
	return current().command_transition_image(
			cmd, image, current_layout, new_layout, base_mip_level, level_count);
}

// =========================================================================
// Utility
// =========================================================================

Res<> command_begin_label(CommandBuffer cmd, const char* name, Color color) {
	return current().command_begin_label(cmd, name, color);
}

Res<> command_end_label(CommandBuffer cmd) { return current().command_end_label(cmd); }

Res<> set_debug_name(ObjectType type, void* handle, const char* name) {
	return current().set_debug_name(type, handle, name);
}

} // namespace gpukit
