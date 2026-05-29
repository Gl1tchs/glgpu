#ifdef GPUKIT_EXPOSE_VULKAN
#ifndef GPUKIT_VULKAN_H
#define GPUKIT_VULKAN_H

#include <vulkan/vulkan.h>

#include "gpukit/export.h"
#include "gpukit/result.h"
#include "gpukit/types.h"

namespace gpukit::vk {

GPUKIT_API VkInstance vulkan_get_instance();
GPUKIT_API VkPhysicalDevice vulkan_get_physical_device();
GPUKIT_API VkDevice vulkan_get_device();

inline VkFormat vulkan_format(DataFormat format) { return static_cast<VkFormat>(format); }
inline DataFormat gpukit_format(VkFormat format) { return static_cast<DataFormat>(format); }

inline VkFence vulkan_fence(Fence fence) { return (VkFence)fence; }
inline VkSemaphore vulkan_semaphore(Semaphore semaphore) { return (VkSemaphore)semaphore; }
inline VkCommandPool vulkan_command_pool(CommandPool pool) { return (VkCommandPool)pool; }
inline VkCommandBuffer vulkan_command_buffer(CommandBuffer cmd) { return (VkCommandBuffer)cmd; }
inline VkFramebuffer vulkan_framebuffer(FrameBuffer fb) { return (VkFramebuffer)fb; }
inline VkSampler vulkan_sampler(Sampler sampler) { return (VkSampler)sampler; }

struct VulkanImageInfo {
	VkImage image;
	VkImageView image_view;
	VkFormat format;
	VkExtent3D extent;
	VkImageUsageFlags usage;
	uint32_t mip_levels;
	uint32_t array_layers;
};

GPUKIT_API Res<VulkanImageInfo> vulkan_get_image_info(Image image);

struct VulkanBufferInfo {
	VkBuffer buffer;
	VkBufferView view; // VK_NULL_HANDLE unless the buffer is a texel buffer
	uint64_t size;
};

GPUKIT_API Res<VulkanBufferInfo> vulkan_get_buffer_info(Buffer buffer);

struct VulkanPipelineInfo {
	VkPipeline pipeline;
};

GPUKIT_API Res<VulkanPipelineInfo> vulkan_get_pipeline_info(Pipeline pipeline);

struct VulkanShaderInfo {
	VkPipelineLayout pipeline_layout;
	const VkDescriptorSetLayout* descriptor_set_layouts;
	uint32_t descriptor_set_layout_count;
	const VkPipelineShaderStageCreateInfo* stage_create_infos;
	uint32_t stage_count;
};

GPUKIT_API Res<VulkanShaderInfo> vulkan_get_shader_info(Shader shader);

struct VulkanUniformSetInfo {
	VkDescriptorSet descriptor_set;
	VkDescriptorPool descriptor_pool;
	bool bindless;
	uint32_t set_index;
};

GPUKIT_API Res<VulkanUniformSetInfo> vulkan_get_uniform_set_info(UniformSet set);

struct VulkanRenderPassInfo {
	VkRenderPass render_pass;
};

GPUKIT_API Res<VulkanRenderPassInfo> vulkan_get_render_pass_info(RenderPass render_pass);

struct VulkanSwapchainInfo {
	VkSwapchainKHR swapchain;
	VkFormat format;
	VkColorSpaceKHR color_space;
	VkExtent2D extent;
	VkSurfaceTransformFlagBitsKHR pre_transform;
	uint32_t image_count;
	uint32_t current_image_index;
};

GPUKIT_API Res<VulkanSwapchainInfo> vulkan_get_swapchain_info(Swapchain swapchain);

struct VulkanQueryPoolInfo {
	VkQueryPool pool;
	uint32_t count;
};

GPUKIT_API Res<VulkanQueryPoolInfo> vulkan_get_query_pool_info(QueryPool query_pool);

struct VulkanQueueInfo {
	VkQueue queue;
	uint32_t queue_family;
};

GPUKIT_API Res<VulkanQueueInfo> vulkan_get_queue_info(CommandQueue queue);

} // namespace gpukit::vk

#endif // GPUKIT_VULKAN_H
#endif // GPUKIT_EXPOSE_VULKAN
