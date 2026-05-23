#pragma once

#include "glgpu/deletion_queue.h"
#include "glgpu/device.h"
#include "glgpu/types.h"
#include "glgpu/versatile_resource.h"

#include "platform/vulkan/vk_common.h"

#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace gl {

// Sanity checks
static_assert(sizeof(ImageSubresourceLayers) == sizeof(VkImageSubresourceLayers));
static_assert(sizeof(ImageResolve) == sizeof(VkImageResolve));
static_assert(sizeof(BufferCopyRegion) == sizeof(VkBufferCopy));
static_assert(sizeof(BufferImageCopyRegion) == sizeof(VkBufferImageCopy));

class VulkanDevice : public Device {
public:
	VulkanDevice() = default;
	virtual ~VulkanDevice();

	Res<> init(const DeviceCreateInfo& info) override;

	// =========================================================================
	// Device & Surface
	// =========================================================================

	Res<> device_wait() override;

	Res<> attach_surface(void* connection_handle, void* window_handle) override;

	uint32_t get_max_msaa_samples() const override;

	uint32_t get_max_bindless_instances() const override;

	bool is_swapchain_supported() override;

	NativeContext get_native_context() const override;

	RenderAPI get_api() const override;

	// Command Queue
	Res<CommandQueue> queue_get(QueueType type) override;

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

	Res<Buffer> buffer_create(
			uint64_t size, BufferUsageFlags usage, MemoryAllocationType allocation_type) override;

	Res<> buffer_free(Buffer buffer) override;

	Res<BufferDeviceAddress> buffer_get_device_address(Buffer buffer) override;

	Res<uint8_t*> buffer_map(Buffer buffer) override;

	Res<> buffer_unmap(Buffer buffer) override;

	Res<> buffer_invalidate(Buffer buffer) override;

	Res<> buffer_flush(Buffer buffer) override;

	// Image
	struct VulkanImage {
		VkImage vk_image;
		VkImageView vk_image_view;
		VmaAllocation allocation;
		VkExtent3D image_extent;
		VkFormat image_format;
		uint32_t mip_levels;
		VkImageUsageFlags image_usage;
	};

	Res<Image> image_create(const ImageCreateInfo& info) override;

	Res<> image_free(Image image) override;

	Res<Vec3u> image_get_size(Image image) override;

	Res<DataFormat> image_get_format(Image image) override;

	Res<uint32_t> image_get_mip_levels(Image image) override;

	Res<ImageUsageFlags> image_get_image_usage(Image image) override;

	// Sampler
	Res<Sampler> sampler_create(const SamplerCreateInfo& info) override;

	Res<> sampler_free(Sampler sampler) override;

	// =========================================================================
	// Shader & Pipelines
	// =========================================================================

	// Shader
	struct ReflectedBinding {
		uint32_t binding;
		VkDescriptorType type;
		uint32_t count;
		std::string name;
	};

	struct VulkanShader {
		std::vector<VkPipelineShaderStageCreateInfo> stage_create_infos;
		uint32_t push_constant_stages = 0;
		std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
		VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

		std::vector<ShaderInterfaceVariable> vertex_input_variables;
		size_t shader_hash;

		std::map<uint32_t, std::map<uint32_t, ReflectedBinding>> reflected_bindings;
	};

	Res<Shader> shader_create(VectorView<SpirvEntry> shaders) override;

	Res<Shader> shader_create(const char* vertex_filepath, const char* fragment_filepath) override;

	Res<Shader> shader_create(const char* compute_filepath) override;

	Res<> shader_free(Shader shader) override;

	Res<std::vector<ShaderInterfaceVariable>> shader_get_vertex_inputs(Shader shader) override;

	Res<std::vector<ShaderResourceInfo>> shader_get_resources(Shader shader) override;

	// Pipeline
	struct VulkanPipeline {
		VkPipeline vk_pipeline;
		VkPipelineCache vk_pipeline_cache;
		size_t shader_hash;
	};

	Res<Pipeline> graphics_pipeline_create(const GraphicsPipelineCreateInfo& info) override;

	Res<Pipeline> compute_pipeline_create(Shader shader) override;

	Res<> pipeline_free(Pipeline pipeline) override;

	// UniformSet
	static const uint32_t MAX_UNIFORM_POOL_ELEMENT = 65535;

	struct DescriptorSetPoolKey {
		uint16_t uniform_type[static_cast<uint32_t>(ShaderUniformType::MAX)] = {};

		bool operator<(const DescriptorSetPoolKey& other) const {
			return memcmp(uniform_type, other.uniform_type, sizeof(uniform_type)) < 0;
		}
	};

	using DescriptorSetPools =
			std::map<DescriptorSetPoolKey, std::unordered_map<VkDescriptorPool, uint32_t>>;

	struct VulkanUniformSet {
		VkDescriptorSet vk_descriptor_set = VK_NULL_HANDLE;
		VkDescriptorPool vk_descriptor_pool = VK_NULL_HANDLE;
		DescriptorSetPoolKey pool_key;

		// Bindless resources
		bool bindless = false;

		// Reflection metadata
		VulkanShader* shader = nullptr;
		uint32_t set_index = 0;
	};

	Res<UniformSet> uniform_set_create(
			VectorView<ShaderUniform> uniforms, Shader shader, uint32_t set_index) override;

	Res<UniformSet> uniform_set_create(Shader shader, uint32_t set_index) override;

	Res<> uniform_set_free(UniformSet uniform_set) override;

	Res<UniformSet> uniform_set_create_bindless(
			Shader shader, uint32_t set_index, uint32_t binding_index, uint32_t max_count) override;

	Res<> uniform_set_update_texture(UniformSet set, uint32_t binding, uint32_t array_index,
			Image image, Sampler sampler) override;

	Res<> uniform_set_update_sampled_image(
			UniformSet set, uint32_t binding, uint32_t array_index, Image image) override;

	Res<> uniform_set_update_storage_image(
			UniformSet set, uint32_t binding, uint32_t array_index, Image image) override;

	Res<> uniform_set_update_buffer(
			UniformSet set, uint32_t binding, uint32_t array_index, Buffer buffer) override;

	// =========================================================================
	// Render Pass & Framebuffer
	// =========================================================================

	// Render pass
	struct VulkanRenderPass {
		VkRenderPass vk_render_pass;
		std::vector<RenderPassAttachment> attachments;
	};

	Res<RenderPass> render_pass_create(VectorView<RenderPassAttachment> attachments,
			VectorView<SubpassInfo> subpasses) override;

	Res<> render_pass_destroy(RenderPass render_pass) override;

	// Frame Buffer
	Res<FrameBuffer> frame_buffer_create(
			RenderPass render_pass, VectorView<Image> attachments, const Vec2u& extent) override;

	Res<> frame_buffer_destroy(FrameBuffer frame_buffer) override;

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

	Res<Swapchain> swapchain_create() override;

	Res<> swapchain_resize(
			CommandQueue cmd_queue, Swapchain swapchain, Vec2u size, bool vsync = false) override;

	Res<size_t> swapchain_get_image_count(Swapchain swapchain) override;

	Res<std::vector<Image>> swapchain_get_images(Swapchain swapchain) override;

	Res<Image> swapchain_acquire_image(
			Swapchain swapchain, Semaphore semaphore, uint32_t* o_image_index) override;

	Res<Vec2u> swapchain_get_extent(Swapchain swapchain) override;

	Res<DataFormat> swapchain_get_format(Swapchain swapchain) override;

	Res<> swapchain_free(Swapchain swapchain) override;

	// =========================================================================
	// Synchronization
	// =========================================================================

	Fence fence_create(bool create_signaled = true) override;

	Res<> fence_free(Fence fence) override;

	Res<> fence_wait(Fence fence) override;

	Res<> fence_reset(Fence fence) override;

	Semaphore semaphore_create() override;

	Res<> semaphore_free(Semaphore semaphore) override;

	// =========================================================================
	// Command Submission & Recording
	// =========================================================================

	struct VulkanQueue {
		VkQueue queue;
		uint32_t queue_family;
		std::mutex mutex;
	};

	Res<> queue_submit(CommandQueue queue, CommandBuffer cmd, Fence fence = GL_NULL_HANDLE,
			Semaphore wait_semaphore = GL_NULL_HANDLE,
			Semaphore signal_semaphore = GL_NULL_HANDLE) override;

	Res<> queue_present(CommandQueue queue, Swapchain swapchain,
			Semaphore wait_semaphore = GL_NULL_HANDLE) override;

	Res<> command_immediate_submit(std::function<void(CommandBuffer cmd)>&& function,
			QueueType queue_type = QueueType::TRANSFER) override;

	Res<CommandPool> command_pool_create(CommandQueue queue) override;

	Res<> command_pool_free(CommandPool command_pool) override;

	Res<CommandBuffer> command_pool_allocate(CommandPool command_pool) override;

	Res<std::vector<CommandBuffer>> command_pool_allocate(
			CommandPool command_pool, const uint32_t count) override;

	Res<> command_pool_reset(CommandPool command_pool) override;

	Res<> command_begin(CommandBuffer cmd) override;

	Res<> command_end(CommandBuffer cmd) override;

	Res<> command_reset(CommandBuffer cmd) override;

	Res<> command_begin_render_pass(CommandBuffer cmd, RenderPass render_pass,
			FrameBuffer framebuffer, const Vec2u& draw_extent,
			Color clear_color = COLOR_GRAY) override;

	Res<> command_end_render_pass(CommandBuffer cmd) override;

	Res<> command_begin_rendering(CommandBuffer cmd, const Vec2u& draw_extent,
			VectorView<RenderingAttachment> color_attachments,
			Image depth_attachment = GL_NULL_HANDLE) override;

	Res<> command_end_rendering(CommandBuffer cmd) override;

	// image layout must be ImageLayout::GENERAL
	Res<> command_clear_color(CommandBuffer cmd, Image image, const Color& clear_color,
			ImageAspectFlags image_aspect = IMAGE_ASPECT_COLOR_BIT) override;

	Res<> command_bind_graphics_pipeline(CommandBuffer cmd, Pipeline pipeline) override;

	Res<> command_bind_compute_pipeline(CommandBuffer cmd, Pipeline pipeline) override;

	Res<> command_bind_vertex_buffers(CommandBuffer cmd, uint32_t first_binding,
			VectorView<Buffer> vertex_buffers, VectorView<uint64_t> offsets) override;

	Res<> command_bind_index_buffer(
			CommandBuffer cmd, Buffer index_buffer, uint64_t offset, IndexType index_type) override;

	Res<> command_draw(CommandBuffer cmd, uint32_t vertex_count, uint32_t instance_count = 1,
			uint32_t first_vertex = 0, uint32_t first_instance = 0) override;

	Res<> command_draw_indexed(CommandBuffer cmd, uint32_t index_count, uint32_t instance_count = 1,
			uint32_t first_index = 0, int32_t vertex_offset = 0,
			uint32_t first_instance = 0) override;

	Res<> command_draw_indexed_indirect(CommandBuffer cmd, Buffer buffer, uint64_t offset,
			uint32_t draw_count, uint32_t stride) override;

	Res<> command_dispatch(CommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y,
			uint32_t group_count_z) override;

	Res<> command_bind_uniform_sets(CommandBuffer cmd, Shader shader, uint32_t first_set,
			VectorView<UniformSet> uniform_sets,
			PipelineType type = PipelineType::GRAPHICS) override;

	Res<> command_push_constants(CommandBuffer cmd, Shader shader, uint64_t offset, uint32_t size,
			const void* push_constants) override;

	Res<> command_set_viewport(CommandBuffer cmd, const Vec2u& size) override;

	Res<> command_set_scissor(
			CommandBuffer cmd, const Vec2u& size, const Vec2u& offset = { 0, 0 }) override;

	Res<> command_set_depth_bias(CommandBuffer cmd, float depth_bias_constant_factor,
			float depth_bias_clamp, float depth_bias_slope_factor) override;

	Res<> command_buffer_memory_barrier(CommandBuffer cmd, BufferUsageFlags src_usage,
			BufferUsageFlags dst_usage, Buffer buffer) override;

	Res<> command_pipeline_barrier(CommandBuffer cmd, VectorView<BufferBarrier> buffer_barriers,
			VectorView<ImageBarrier> image_barriers) override;

	Res<> command_copy_buffer(CommandBuffer cmd, Buffer src_buffer, Buffer dst_buffer,
			VectorView<BufferCopyRegion> regions) override;

	Res<> command_copy_buffer_to_image(CommandBuffer cmd, Buffer src_buffer, Image dst_image,
			VectorView<BufferImageCopyRegion> regions) override;

	Res<> command_copy_image_to_image(CommandBuffer cmd, Image src_image, Image dst_image,
			const Vec2u& src_extent, const Vec2u& dst_extent, uint32_t src_mip_level = 0,
			uint32_t dst_mip_level = 0) override;

	Res<> command_transition_image(CommandBuffer cmd, Image image, ImageLayout current_layout,
			ImageLayout new_layout, uint32_t base_mip_level = 0,
			uint32_t level_count = GL_REMAINING_MIP_LEVELS) override;

	// Utility functions

	Res<> command_begin_label(CommandBuffer cmd, const char* name, Color color) override;
	Res<> command_end_label(CommandBuffer cmd) override;

	Res<> set_debug_name(ObjectType type, void* handle, const char* name) override;

private:
	// Vulkan helpers

	bool _check_validation_layer_support();

	struct QueueFamilyIndices {
		std::optional<uint32_t> graphics_family;
		std::optional<uint32_t> present_family;
		std::optional<uint32_t> transfer_family;
		std::optional<uint32_t> compute_family;

		bool is_complete(bool surface_support, bool distinct_compute_queue) const {
			const bool basic = graphics_family.has_value() && transfer_family.has_value();
			return basic && (surface_support ? present_family.has_value() : true) &&
					(distinct_compute_queue ? compute_family.has_value() : true);
		}
	};

	static uint32_t _rate_device_suitability(VkPhysicalDevice physical_device,
			const std::vector<const char*>& required_extensions,
			DeviceFeatureFlags required_features, VkSurfaceKHR surface = VK_NULL_HANDLE);

	static QueueFamilyIndices _find_queue_families(VkPhysicalDevice device,
			DeviceFeatureFlags flags, VkSurfaceKHR surface = VK_NULL_HANDLE);

	static bool _check_device_extension_support(
			VkPhysicalDevice device, const std::vector<const char*>& extensions);

	struct SurfaceCapabilities {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> present_modes;
	};

	static std::optional<SurfaceCapabilities> _check_surface_capabilities(
			VkPhysicalDevice physical_device, VkSurfaceKHR surface);

	bool _create_surface_platform_specific(void* connection, void* window);

	static VkResult _create_debug_utils_messenger_ext(VkInstance instance,
			const VkDebugUtilsMessengerCreateInfoEXT* info, const VkAllocationCallbacks* allocator,
			VkDebugUtilsMessengerEXT* debug_messenger);

	static void _destroy_debug_utils_messenger_ext(VkInstance instance,
			VkDebugUtilsMessengerEXT debug_messenger, const VkAllocationCallbacks* allocator);

	// API Helpers

	VulkanImage* _image_create(VkFormat format, VkExtent3D size, VkImageUsageFlags usage,
			bool mipmapped, VkSampleCountFlagBits samples);

	void _generate_image_mipmaps(CommandBuffer cmd, Image image, Vec2u size);

	void _swapchain_release(VulkanSwapchain* swapchain);

	VmaPool _find_or_create_small_allocs_pool(uint32_t mem_type_index);

	VkDescriptorPool _uniform_pool_find_or_create(const DescriptorSetPoolKey& key);

	void _uniform_pool_unreference(
			const DescriptorSetPoolKey& key, VkDescriptorPool vk_descriptor_pool);

private:
	using VersatileResource = VersatileResourceTemplate<VulkanBuffer, VulkanImage, VulkanShader,
			VulkanUniformSet, VulkanPipeline>;

	VkInstance _instance;
	VkDevice _device;

	VkPhysicalDevice _physical_device;
	VkPhysicalDeviceProperties _physical_device_properties;
	VkPhysicalDeviceFeatures _physical_device_features;
	bool _swapchain_supported;

	// Debug ressources
	VkDebugUtilsMessengerEXT _debug_messenger = VK_NULL_HANDLE;

	PFN_vkCmdBeginDebugUtilsLabelEXT _vkCmdBeginDebugUtilsLabelEXT;
	PFN_vkCmdEndDebugUtilsLabelEXT _vkCmdEndDebugUtilsLabelEXT;
	PFN_vkSetDebugUtilsObjectNameEXT _vkSetDebugUtilsObjectNameEXT;

	VkSurfaceKHR _surface = VK_NULL_HANDLE;

	VulkanQueue _graphics_queue;
	VulkanQueue _transfer_queue;
	VulkanQueue _present_queue;
	VulkanQueue _compute_queue;

	static const uint32_t SMALL_ALLOCATION_MAX_SIZE = 4096;

	VmaAllocator _allocator = nullptr;
	std::unordered_map<uint32_t, VmaPool> _small_allocs_pools;

	DescriptorSetPools _descriptor_set_pools;

	PagedAllocator<VersatileResource> _resources_allocator;

	// immediate commands
	struct ImmediateBuffer {
		Fence fence;
		CommandPool command_pool;
		CommandBuffer command_buffer;
	};

	ImmediateBuffer _imm_transfer;
	std::mutex _imm_cmd_transfer_mutex;

	ImmediateBuffer _imm_graphics;
	std::mutex _imm_cmd_graphics_mutex;

	DeletionQueue _deletion_queue;

	std::shared_ptr<std::mutex> _get_pool_mutex(VkCommandPool pool);
	std::shared_ptr<std::mutex> _get_pool_mutex_from_cmd(VkCommandBuffer cmd);

	// Thread safety for command pools and command buffers
	std::mutex _command_pools_mutex;
	std::unordered_map<VkCommandPool, std::shared_ptr<std::mutex>> _command_pools;
	std::unordered_map<VkCommandBuffer, VkCommandPool> _command_buffer_parents;
};

} // namespace gl
