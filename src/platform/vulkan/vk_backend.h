#pragma once

#include "glgpu/backend.h"
#include "glgpu/deletion_queue.h"
#include "glgpu/types.h"
#include "glgpu/versatile_resource.h"

#include "platform/vulkan/vk_common.h"

namespace gl {

// Sanity checks
static_assert(sizeof(ImageSubresourceLayers) == sizeof(VkImageSubresourceLayers));
static_assert(sizeof(ImageResolve) == sizeof(VkImageResolve));
static_assert(sizeof(BufferCopyRegion) == sizeof(VkBufferCopy));
static_assert(sizeof(BufferImageCopyRegion) == sizeof(VkBufferImageCopy));

class VulkanRenderBackend : public RenderBackend {
public:
	VulkanRenderBackend(const RenderBackendCreateInfo& p_info);
	virtual ~VulkanRenderBackend();

	// =========================================================================
	// Device & Surface
	// =========================================================================

	void device_wait() override;

	Error attach_surface(void* p_connection_handle, void* p_window_handle) override;

	bool is_swapchain_supported() override;

	uint32_t get_max_msaa_samples() const override;

	// Command Queue
	CommandQueue queue_get(QueueType p_type) override;

	// =========================================================================
	// Resource Management (Buffers & Images)
	// =========================================================================

	// Buffer
	struct VulkanBuffer {
		VkBuffer vk_buffer;
		struct {
			VmaAllocation handle;
			uint64_t size = UINT64_MAX;
		} allocation;
		uint64_t size = 0;
		VkBufferView vk_view = VK_NULL_HANDLE;
	};

	Buffer buffer_create(uint64_t p_size, BufferUsageFlags p_usage,
			MemoryAllocationType p_allocation_type) override;

	void buffer_free(Buffer p_buffer) override;

	BufferDeviceAddress buffer_get_device_address(Buffer p_buffer) override;

	uint8_t* buffer_map(Buffer p_buffer) override;

	void buffer_unmap(Buffer p_buffer) override;

	void buffer_invalidate(Buffer p_buffer) override;

	void buffer_flush(Buffer p_buffer) override;

	// Image
	struct VulkanImage {
		VkImage vk_image;
		VkImageView vk_image_view;
		VmaAllocation allocation;
		VkExtent3D image_extent;
		VkFormat image_format;
		uint32_t mip_levels;
	};

	// Signature updated to use ImageCreateInfo struct
	Image image_create(const ImageCreateInfo& p_info) override;

	void image_free(Image p_image) override;

	Vec3u image_get_size(Image p_image) override;

	DataFormat image_get_format(Image p_image) override;

	uint32_t image_get_mip_levels(Image p_image) override;

	// Sampler
	// Signature updated to use SamplerCreateInfo struct
	Sampler sampler_create(const SamplerCreateInfo& p_info) override;

	void sampler_free(Sampler p_sampler) override;

	// =========================================================================
	// Shader & Pipelines
	// =========================================================================

	// Shader
	struct VulkanShader {
		std::vector<VkPipelineShaderStageCreateInfo> stage_create_infos;
		uint32_t push_constant_stages = 0;
		std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
		VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

		std::vector<ShaderInterfaceVariable> vertex_input_variables;
		size_t shader_hash;
	};

	Shader shader_create_from_bytecode(const std::vector<SpirvEntry>& p_shaders) override;

	void shader_free(Shader p_shader) override;

	std::vector<ShaderInterfaceVariable> shader_get_vertex_inputs(Shader p_shader) override;

	// Pipeline
	struct VulkanPipeline {
		VkPipeline vk_pipeline;
		VkPipelineCache vk_pipeline_cache;
		size_t shader_hash;
	};

	// Consolidated two overloads into one using the RenderPipelineCreateInfo struct
	Pipeline render_pipeline_create(const RenderPipelineCreateInfo& p_info) override;

	Pipeline compute_pipeline_create(Shader p_shader) override;

	void pipeline_free(Pipeline p_pipeline) override;

	// UniformSet
	static const uint32_t MAX_UNIFORM_POOL_ELEMENT = 65535;

	struct DescriptorSetPoolKey {
		uint16_t uniform_type[static_cast<uint32_t>(ShaderUniformType::MAX)] = {};

		bool operator<(const DescriptorSetPoolKey& p_other) const {
			return memcmp(uniform_type, p_other.uniform_type, sizeof(uniform_type)) < 0;
		}
	};

	using DescriptorSetPools =
			std::map<DescriptorSetPoolKey, std::unordered_map<VkDescriptorPool, uint32_t>>;

	struct VulkanUniformSet {
		VkDescriptorSet vk_descriptor_set = VK_NULL_HANDLE;
		VkDescriptorPool vk_descriptor_pool = VK_NULL_HANDLE;
		DescriptorSetPoolKey pool_key;
	};

	UniformSet uniform_set_create(
			std::vector<ShaderUniform> p_uniforms, Shader p_shader, uint32_t p_set_index) override;

	void uniform_set_free(UniformSet p_uniform_set) override;

	// =========================================================================
	// Render Pass & Framebuffer
	// =========================================================================

	// Render pass
	struct VulkanRenderPass {
		VkRenderPass vk_render_pass;
		std::vector<RenderPassAttachment> attachments;
	};

	RenderPass render_pass_create(std::vector<RenderPassAttachment> p_attachments,
			std::vector<SubpassInfo> p_subpasses) override;

	void render_pass_destroy(RenderPass p_render_pass) override;

	// Frame Buffer
	FrameBuffer frame_buffer_create(RenderPass p_render_pass, std::vector<Image> p_attachments,
			const Vec2u& p_extent) override;

	void frame_buffer_destroy(FrameBuffer p_frame_buffer) override;

	// =========================================================================
	// Swapchain
	// =========================================================================

	struct VulkanSwapchain {
		VkSwapchainKHR vk_swapchain = VK_NULL_HANDLE;
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkColorSpaceKHR color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		VkExtent2D extent;
		std::vector<VulkanImage> images;
		uint32_t image_index;
		bool initialized;
	};

	Swapchain swapchain_create() override;

	void swapchain_resize(CommandQueue p_cmd_queue, Swapchain p_swapchain, Vec2u size,
			bool p_vsync = false) override;

	size_t swapchain_get_image_count(Swapchain p_swapchain) override;

	std::vector<Image> swapchain_get_images(Swapchain p_swapchain) override;

	// Result type updated to use the global Error enum
	Result<Image, Error> swapchain_acquire_image(
			Swapchain p_swapchain, Semaphore p_semaphore, uint32_t* o_image_index) override;

	Vec2u swapchain_get_extent(Swapchain p_swapchain) override;

	DataFormat swapchain_get_format(Swapchain p_swapchain) override;

	void swapchain_free(Swapchain p_swapchain) override;

	// =========================================================================
	// Synchronization
	// =========================================================================

	Fence fence_create(bool p_create_signaled = true) override;

	void fence_free(Fence p_fence) override;

	void fence_wait(Fence p_fence) override;

	void fence_reset(Fence p_fence) override;

	Semaphore semaphore_create() override;

	void semaphore_free(Semaphore p_semaphore) override;

	// =========================================================================
	// Command Submission & Recording
	// =========================================================================

	struct VulkanQueue {
		VkQueue queue;
		uint32_t queue_family;
		std::mutex mutex;
	};

	void queue_submit(CommandQueue p_queue, CommandBuffer p_cmd, Fence p_fence = GL_NULL_HANDLE,
			Semaphore p_wait_semaphore = GL_NULL_HANDLE,
			Semaphore p_signal_semaphore = GL_NULL_HANDLE) override;

	bool queue_present(CommandQueue p_queue, Swapchain p_swapchain,
			Semaphore p_wait_semaphore = GL_NULL_HANDLE) override;

	void command_immediate_submit(std::function<void(CommandBuffer p_cmd)>&& p_function,
			QueueType p_queue_type = QueueType::TRANSFER) override;

	CommandPool command_pool_create(CommandQueue p_queue) override;

	void command_pool_free(CommandPool p_command_pool) override;

	CommandBuffer command_pool_allocate(CommandPool p_command_pool) override;

	std::vector<CommandBuffer> command_pool_allocate(
			CommandPool p_command_pool, const uint32_t count) override;

	void command_pool_reset(CommandPool p_command_pool) override;

	void command_begin(CommandBuffer p_cmd) override;

	void command_end(CommandBuffer p_cmd) override;

	void command_reset(CommandBuffer p_cmd) override;

	void command_begin_render_pass(CommandBuffer p_cmd, RenderPass p_render_pass,
			FrameBuffer framebuffer, const Vec2u& p_draw_extent,
			Color clear_color = COLOR_GRAY) override;

	void command_end_render_pass(CommandBuffer p_cmd) override;

	void command_begin_rendering(CommandBuffer p_cmd, const Vec2u& p_draw_extent,
			std::vector<RenderingAttachment> p_color_attachments,
			Image p_depth_attachment = GL_NULL_HANDLE) override; // Corrected default value

	void command_end_rendering(CommandBuffer p_cmd) override;

	// image layout must be ImageLayout::GENERAL
	void command_clear_color(CommandBuffer p_cmd, Image p_image, const Color& p_clear_color,
			ImageAspectFlags p_image_aspect = IMAGE_ASPECT_COLOR_BIT) override;

	void command_bind_graphics_pipeline(CommandBuffer p_cmd, Pipeline p_pipeline) override;

	void command_bind_compute_pipeline(CommandBuffer p_cmd, Pipeline p_pipeline) override;

	void command_bind_vertex_buffers(CommandBuffer p_cmd, uint32_t p_first_binding,
			std::vector<Buffer> p_vertex_buffers, std::vector<uint64_t> p_offsets) override;

	void command_bind_index_buffer(CommandBuffer p_cmd, Buffer p_index_buffer, uint64_t p_offset,
			IndexType p_index_type) override;

	void command_draw(CommandBuffer p_cmd, uint32_t p_vertex_count, uint32_t p_instance_count = 1,
			uint32_t p_first_vertex = 0, uint32_t p_first_instance = 0) override;

	void command_draw_indexed(CommandBuffer p_cmd, uint32_t p_index_count,
			uint32_t p_instance_count = 1, uint32_t p_first_index = 0, int32_t p_vertex_offset = 0,
			uint32_t p_first_instance = 0) override;

	void command_draw_indexed_indirect(CommandBuffer p_cmd, Buffer p_buffer, uint64_t p_offset,
			uint32_t p_draw_count, uint32_t p_stride) override;

	void command_dispatch(CommandBuffer p_cmd, uint32_t p_group_count_x, uint32_t p_group_count_y,
			uint32_t p_group_count_z) override;

	void command_bind_uniform_sets(CommandBuffer p_cmd, Shader p_shader, uint32_t p_first_set,
			std::vector<UniformSet> p_uniform_sets,
			PipelineType p_type = PipelineType::GRAPHICS) override;

	void command_push_constants(CommandBuffer p_cmd, Shader p_shader, uint64_t p_offset,
			uint32_t p_size, const void* p_push_constants) override;

	void command_set_viewport(CommandBuffer p_cmd, const Vec2u& size) override;

	void command_set_scissor(
			CommandBuffer p_cmd, const Vec2u& p_size, const Vec2u& p_offset = { 0, 0 }) override;

	void command_set_depth_bias(CommandBuffer p_cmd, float p_depth_bias_constant_factor,
			float p_depth_bias_clamp, float p_depth_bias_slope_factor) override;

	void command_buffer_memory_barrier(CommandBuffer p_cmd, BufferUsageFlags p_src_usage,
			BufferUsageFlags p_dst_usage, Buffer p_buffer) override;

	void command_copy_buffer(CommandBuffer p_cmd, Buffer p_src_buffer, Buffer p_dst_buffer,
			std::vector<BufferCopyRegion> p_regions) override;

	void command_copy_buffer_to_image(CommandBuffer p_cmd, Buffer p_src_buffer, Image p_dst_image,
			std::vector<BufferImageCopyRegion> p_regions) override;

	void command_copy_image_to_image(CommandBuffer p_cmd, Image p_src_image, Image p_dst_image,
			const Vec2u& p_src_extent, const Vec2u& p_dst_extent, uint32_t p_src_mip_level = 0,
			uint32_t p_dst_mip_level = 0) override;

	void command_transition_image(CommandBuffer p_cmd, Image p_image, ImageLayout p_current_layout,
			ImageLayout p_new_layout, uint32_t p_base_mip_level = 0,
			uint32_t p_level_count = GL_REMAINING_MIP_LEVELS) override;

private:
	// Vulkan helpers

	bool _check_validation_layer_support();

	struct QueueFamilyIndices {
		std::optional<uint32_t> graphics_family;
		std::optional<uint32_t> present_family;
		std::optional<uint32_t> transfer_family;
		std::optional<uint32_t> compute_family;

		bool is_complete(bool p_surface_support, bool p_distinct_compute_queue) const {
			const bool basic = graphics_family.has_value() && transfer_family.has_value();
			return basic && (p_surface_support ? present_family.has_value() : true) &&
					(p_distinct_compute_queue ? compute_family.has_value() : true);
		}
	};

	static uint32_t _rate_device_suitability(VkPhysicalDevice p_physical_device,
			const std::vector<const char*>& p_required_extensions,
			RenderBackendFeatureFlags p_required_features, VkSurfaceKHR p_surface = VK_NULL_HANDLE);

	static QueueFamilyIndices _find_queue_families(VkPhysicalDevice p_device,
			RenderBackendFeatureFlags p_flags, VkSurfaceKHR p_surface = VK_NULL_HANDLE);

	static bool _check_device_extension_support(
			VkPhysicalDevice p_device, const std::vector<const char*>& p_extensions);

	struct SurfaceCapabilities {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> present_modes;
	};

	static std::optional<SurfaceCapabilities> _check_surface_capabilities(
			VkPhysicalDevice p_physical_device, VkSurfaceKHR p_surface);

	bool _create_surface_platform_specific(void* p_connection, void* p_window);

	static VkResult _create_debug_utils_messenger_ext(VkInstance p_instance,
			const VkDebugUtilsMessengerCreateInfoEXT* p_info,
			const VkAllocationCallbacks* p_allocator, VkDebugUtilsMessengerEXT* p_debug_messenger);

	static void _destroy_debug_utils_messenger_ext(VkInstance p_instance,
			VkDebugUtilsMessengerEXT p_debug_messenger, const VkAllocationCallbacks* p_allocator);

	// API Helpers

	// Helper signature updated to use internal/Vulkan types
	VulkanImage* _image_create(VkFormat p_format, VkExtent3D p_size, VkImageUsageFlags p_usage,
			bool p_mipmapped, VkSampleCountFlagBits p_samples);

	void _generate_image_mipmaps(CommandBuffer p_cmd, Image p_image, Vec2u p_size);

	void _swapchain_release(VulkanSwapchain* p_swapchain);

	VmaPool _find_or_create_small_allocs_pool(uint32_t p_mem_type_index);

	VkDescriptorPool _uniform_pool_find_or_create(const DescriptorSetPoolKey& p_key);

	void _uniform_pool_unreference(
			const DescriptorSetPoolKey& p_key, VkDescriptorPool p_vk_descriptor_pool);

private:
	using VersatileResource = VersatileResourceTemplate<VulkanBuffer, VulkanImage, VulkanShader,
			VulkanUniformSet, VulkanPipeline>;

	VkInstance instance;
	VkDevice device;

	VkPhysicalDevice physical_device;
	VkPhysicalDeviceProperties physical_device_properties;
	VkPhysicalDeviceFeatures physical_device_features;
	bool swapchain_supported;

	VkDebugUtilsMessengerEXT debug_messenger;

	VkSurfaceKHR surface = VK_NULL_HANDLE;

	VulkanQueue graphics_queue;
	VulkanQueue transfer_queue;
	VulkanQueue present_queue;
	VulkanQueue compute_queue;

	static const uint32_t SMALL_ALLOCATION_MAX_SIZE = 4096;

	VmaAllocator allocator = nullptr;
	std::unordered_map<uint32_t, VmaPool> small_allocs_pools;

	DescriptorSetPools descriptor_set_pools;

	PagedAllocator<VersatileResource> resources_allocator;

	// immediate commands
	struct ImmediateBuffer {
		Fence fence;
		CommandPool command_pool;
		CommandBuffer command_buffer;
	};

	ImmediateBuffer imm_transfer;
	std::mutex imm_cmd_transfer_mutex;

	ImmediateBuffer imm_graphics;
	std::mutex imm_cmd_graphics_mutex;

	DeletionQueue deletion_queue;
};

} // namespace gl
