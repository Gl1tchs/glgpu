#ifndef GPUKIT_EXPOSE_VULKAN
#define GPUKIT_EXPOSE_VULKAN
#endif

#include "gpukit/vulkan.h"

#include "platform/vulkan/vk_device.h"

namespace gpukit::vk {

VkInstance vulkan_get_instance() {
	NativeContext ctx = get_native_context();
	return static_cast<VkInstance>(ctx.instance);
}

VkPhysicalDevice vulkan_get_physical_device() {
	NativeContext ctx = get_native_context();
	return static_cast<VkPhysicalDevice>(ctx.physical_device);
}

VkDevice vulkan_get_device() {
	NativeContext ctx = get_native_context();
	return static_cast<VkDevice>(ctx.device);
}

Res<VulkanImageInfo> vulkan_get_image_info(Image image) {
	if (!image)
		return make_err<VulkanImageInfo>(Error::INVALID_HANDLE);
	auto* vk = reinterpret_cast<VulkanDevice::VulkanImage*>(image);
	return VulkanImageInfo{
		.image = vk->vk_image,
		.image_view = vk->vk_image_view,
		.format = vk->image_format,
		.extent = vk->image_extent,
		.usage = vk->image_usage,
		.mip_levels = vk->mip_levels,
		.array_layers = vk->array_layers,
	};
}

Res<VulkanBufferInfo> vulkan_get_buffer_info(Buffer buffer) {
	if (!buffer)
		return make_err<VulkanBufferInfo>(Error::INVALID_HANDLE);
	auto* vk = reinterpret_cast<VulkanDevice::VulkanBuffer*>(buffer);
	return VulkanBufferInfo{
		.buffer = vk->vk_buffer,
		.view = vk->vk_view,
		.size = vk->size,
	};
}

Res<VulkanPipelineInfo> vulkan_get_pipeline_info(Pipeline pipeline) {
	if (!pipeline)
		return make_err<VulkanPipelineInfo>(Error::INVALID_HANDLE);
	auto* vk = reinterpret_cast<VulkanDevice::VulkanPipeline*>(pipeline);
	return VulkanPipelineInfo{ .pipeline = vk->vk_pipeline };
}

Res<VulkanShaderInfo> vulkan_get_shader_info(Shader shader) {
	if (!shader)
		return make_err<VulkanShaderInfo>(Error::INVALID_HANDLE);
	auto* vk = reinterpret_cast<VulkanDevice::VulkanShader*>(shader);
	return VulkanShaderInfo{
		.pipeline_layout = vk->pipeline_layout,
		.descriptor_set_layouts = vk->descriptor_set_layouts.data(),
		.descriptor_set_layout_count = static_cast<uint32_t>(vk->descriptor_set_layouts.size()),
		.stage_create_infos = vk->stage_create_infos.data(),
		.stage_count = static_cast<uint32_t>(vk->stage_create_infos.size()),
	};
}

Res<VulkanUniformSetInfo> vulkan_get_uniform_set_info(UniformSet set) {
	if (!set)
		return make_err<VulkanUniformSetInfo>(Error::INVALID_HANDLE);
	auto* vk = reinterpret_cast<VulkanDevice::VulkanUniformSet*>(set);
	return VulkanUniformSetInfo{
		.descriptor_set = vk->vk_descriptor_set,
		.descriptor_pool = vk->vk_descriptor_pool,
		.bindless = vk->bindless,
		.set_index = vk->set_index,
	};
}

Res<VulkanRenderPassInfo> vulkan_get_render_pass_info(RenderPass render_pass) {
	if (!render_pass)
		return make_err<VulkanRenderPassInfo>(Error::INVALID_HANDLE);
	auto* vk = reinterpret_cast<VulkanDevice::VulkanRenderPass*>(render_pass);
	return VulkanRenderPassInfo{ .render_pass = vk->vk_render_pass };
}

Res<VulkanSwapchainInfo> vulkan_get_swapchain_info(Swapchain swapchain) {
	if (!swapchain)
		return make_err<VulkanSwapchainInfo>(Error::INVALID_HANDLE);
	auto* vk = reinterpret_cast<VulkanDevice::VulkanSwapchain*>(swapchain);
	return VulkanSwapchainInfo{
		.swapchain = vk->vk_swapchain,
		.format = vk->format,
		.color_space = vk->color_space,
		.extent = vk->extent,
		.pre_transform = vk->pre_transform,
		.image_count = static_cast<uint32_t>(vk->images.size()),
		.current_image_index = vk->image_index,
	};
}

Res<VulkanQueryPoolInfo> vulkan_get_query_pool_info(QueryPool query_pool) {
	if (!query_pool)
		return make_err<VulkanQueryPoolInfo>(Error::INVALID_HANDLE);
	auto* vk = reinterpret_cast<VulkanDevice::VulkanQueryPool*>(query_pool);
	return VulkanQueryPoolInfo{ .pool = vk->vk_query_pool, .count = vk->count };
}

Res<VulkanQueueInfo> vulkan_get_queue_info(CommandQueue queue) {
	if (!queue)
		return make_err<VulkanQueueInfo>(Error::INVALID_HANDLE);
	auto* vk = reinterpret_cast<VulkanDevice::VulkanQueue*>(queue);
	return VulkanQueueInfo{ .queue = vk->queue, .queue_family = vk->queue_family };
}

} // namespace gpukit::vk
