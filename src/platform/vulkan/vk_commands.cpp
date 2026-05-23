#include "platform/vulkan/vk_device.h"

#include <vulkan/vulkan_core.h>

namespace gpukit {

Res<> VulkanDevice::command_immediate_submit(
		std::function<void(CommandBuffer cmd)>&& function, QueueType queue_type) {
	std::mutex& cmd_mutex =
			(queue_type == QueueType::TRANSFER) ? _imm_cmd_transfer_mutex : _imm_cmd_graphics_mutex;

	std::scoped_lock lock(cmd_mutex);

	ImmediateBuffer* imm = (queue_type == QueueType::TRANSFER) ? &_imm_transfer : &_imm_graphics;

	VK_CHECK_RET(vkResetFences(_device, 1, (VkFence*)&imm->fence), Error::FENCE_TIMEOUT);

	// Reset internal command buffer
	VkResult reset_res = vkResetCommandBuffer((VkCommandBuffer)imm->command_buffer, 0);
	if (reset_res != VK_SUCCESS)
		return Error::COMMAND_SUBMISSION_FAILED;

	// Begin recording
	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK_RET(vkBeginCommandBuffer((VkCommandBuffer)imm->command_buffer, &begin_info),
			Error::COMMAND_SUBMISSION_FAILED);

	// Execute user function
	function(imm->command_buffer);

	// End recording
	VK_CHECK_RET(vkEndCommandBuffer((VkCommandBuffer)imm->command_buffer),
			Error::COMMAND_SUBMISSION_FAILED);

	// Submit
	CommandQueue target_queue_res = (queue_type == QueueType::TRANSFER)
			? queue_get(QueueType::TRANSFER).value()
			: queue_get(QueueType::GRAPHICS).value();

	Res<> submit_res = queue_submit(target_queue_res, imm->command_buffer, imm->fence);
	if (submit_res.is_error())
		return submit_res.error();

	// Wait for fence
	VK_CHECK_RET(vkWaitForFences(_device, 1, (VkFence*)&imm->fence, true, UINT64_MAX),
			Error::FENCE_TIMEOUT);

	return {};
}

std::shared_ptr<std::mutex> VulkanDevice::_get_pool_mutex(VkCommandPool pool) {
	std::lock_guard<std::mutex> lock(_command_pools_mutex);
	auto it = _command_pools.find(pool);
	if (it != _command_pools.end()) {
		return it->second;
	}
	return nullptr;
}

std::shared_ptr<std::mutex> VulkanDevice::_get_pool_mutex_from_cmd(VkCommandBuffer cmd) {
	std::lock_guard<std::mutex> lock(_command_pools_mutex);
	auto it = _command_buffer_parents.find(cmd);
	if (it != _command_buffer_parents.end()) {
		auto pool_it = _command_pools.find(it->second);
		if (pool_it != _command_pools.end()) {
			return pool_it->second;
		}
	}
	return nullptr;
}

Res<CommandPool> VulkanDevice::command_pool_create(CommandQueue queue) {
	VulkanQueue* vk_queue = (VulkanQueue*)queue;
	if (!vk_queue) {
		return make_err<CommandPool>(Error::INVALID_HANDLE);
	}

	VkCommandPoolCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	create_info.queueFamilyIndex = vk_queue->queue_family;
	create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandPool vk_command_pool = VK_NULL_HANDLE;
	VK_CHECK_RET(vkCreateCommandPool(_device, &create_info, nullptr, &vk_command_pool),
			make_err<CommandPool>(Error::INITIALIZATION_FAILED));

	{
		std::lock_guard<std::mutex> lock(_command_pools_mutex);
		_command_pools[vk_command_pool] = std::make_shared<std::mutex>();
	}

	return CommandPool(vk_command_pool);
}

Res<> VulkanDevice::command_pool_free(CommandPool command_pool) {
	if (!command_pool) {
		return {};
	}

	VkCommandPool vk_pool = (VkCommandPool)command_pool;
	std::shared_ptr<std::mutex> pool_mutex = _get_pool_mutex(vk_pool);

	if (pool_mutex) {
		std::lock_guard<std::mutex> pool_lock(*pool_mutex);
		vkDestroyCommandPool(_device, vk_pool, nullptr);
	} else {
		vkDestroyCommandPool(_device, vk_pool, nullptr);
	}

	{
		std::lock_guard<std::mutex> lock(_command_pools_mutex);
		_command_pools.erase(vk_pool);
		for (auto cb_it = _command_buffer_parents.begin(); cb_it != _command_buffer_parents.end(); ) {
			if (cb_it->second == vk_pool) {
				cb_it = _command_buffer_parents.erase(cb_it);
			} else {
				++cb_it;
			}
		}
	}

	return {};
}

Res<CommandBuffer> VulkanDevice::command_pool_allocate(CommandPool command_pool) {
	VkCommandPool vk_pool = (VkCommandPool)command_pool;
	if (!vk_pool) {
		return make_err<CommandBuffer>(Error::INVALID_HANDLE);
	}

	std::shared_ptr<std::mutex> pool_mutex = _get_pool_mutex(vk_pool);

	VkCommandBuffer vk_command_buffer = VK_NULL_HANDLE;
	VkResult res = VK_SUCCESS;

	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.pNext = nullptr;
	alloc_info.commandPool = vk_pool;
	alloc_info.commandBufferCount = 1;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	if (pool_mutex) {
		std::lock_guard<std::mutex> pool_lock(*pool_mutex);
		res = vkAllocateCommandBuffers(_device, &alloc_info, &vk_command_buffer);
	} else {
		res = vkAllocateCommandBuffers(_device, &alloc_info, &vk_command_buffer);
	}

	if (res == VK_SUCCESS && vk_command_buffer != VK_NULL_HANDLE) {
		std::lock_guard<std::mutex> lock(_command_pools_mutex);
		_command_buffer_parents[vk_command_buffer] = vk_pool;
	}

	if (res != VK_SUCCESS) {
		return make_err<CommandBuffer>(Error::OUT_OF_HOST_MEMORY);
	}

	return CommandBuffer(vk_command_buffer);
}

Res<std::vector<CommandBuffer>> VulkanDevice::command_pool_allocate(
		CommandPool command_pool, const uint32_t count) {
	VkCommandPool vk_pool = (VkCommandPool)command_pool;
	if (!vk_pool) {
		return make_err<std::vector<CommandBuffer>>(Error::INVALID_HANDLE);
	}

	std::shared_ptr<std::mutex> pool_mutex = _get_pool_mutex(vk_pool);

	VkCommandBufferAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.pNext = nullptr;
	alloc_info.commandPool = vk_pool;
	alloc_info.commandBufferCount = count;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	std::vector<VkCommandBuffer> vk_command_buffers(count, VK_NULL_HANDLE);
	VkResult res = VK_SUCCESS;

	if (pool_mutex) {
		std::lock_guard<std::mutex> pool_lock(*pool_mutex);
		res = vkAllocateCommandBuffers(_device, &alloc_info, vk_command_buffers.data());
	} else {
		res = vkAllocateCommandBuffers(_device, &alloc_info, vk_command_buffers.data());
	}

	if (res == VK_SUCCESS) {
		std::lock_guard<std::mutex> lock(_command_pools_mutex);
		for (auto cb : vk_command_buffers) {
			if (cb != VK_NULL_HANDLE) {
				_command_buffer_parents[cb] = vk_pool;
			}
		}
	}

	if (res != VK_SUCCESS) {
		return make_err<std::vector<CommandBuffer>>(Error::OUT_OF_HOST_MEMORY);
	}

	std::vector<CommandBuffer> result(count);
	for (uint32_t i = 0; i < count; i++) {
		result[i] = CommandBuffer(vk_command_buffers[i]);
	}
	return result;
}

Res<> VulkanDevice::command_pool_reset(CommandPool command_pool) {
	if (!command_pool) {
		return Error::INVALID_HANDLE;
	}

	VkCommandPool vk_pool = (VkCommandPool)command_pool;
	std::shared_ptr<std::mutex> pool_mutex = _get_pool_mutex(vk_pool);

	VkResult res = VK_SUCCESS;
	if (pool_mutex) {
		std::lock_guard<std::mutex> pool_lock(*pool_mutex);
		res = vkResetCommandPool(_device, vk_pool, VK_COMMAND_POOL_RESET_FLAG_BITS_MAX_ENUM);
	} else {
		res = vkResetCommandPool(_device, vk_pool, VK_COMMAND_POOL_RESET_FLAG_BITS_MAX_ENUM);
	}

	if (res != VK_SUCCESS) {
		return Error::INVALID_OPERATION;
	}

	return {};
}

Res<> VulkanDevice::command_begin(CommandBuffer cmd) {
	if (!cmd) {
		return Error::INVALID_HANDLE;
	}

	VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
	std::shared_ptr<std::mutex> pool_mutex = _get_pool_mutex_from_cmd(vk_cmd);

	VkCommandBufferBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = nullptr;
	begin_info.pInheritanceInfo = nullptr;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VkResult res = VK_SUCCESS;
	if (pool_mutex) {
		std::lock_guard<std::mutex> pool_lock(*pool_mutex);
		res = vkBeginCommandBuffer(vk_cmd, &begin_info);
	} else {
		res = vkBeginCommandBuffer(vk_cmd, &begin_info);
	}

	if (res != VK_SUCCESS) {
		return Error::COMMAND_SUBMISSION_FAILED;
	}

	return {};
}

Res<> VulkanDevice::command_end(CommandBuffer cmd) {
	if (!cmd) {
		return Error::INVALID_HANDLE;
	}

	VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
	std::shared_ptr<std::mutex> pool_mutex = _get_pool_mutex_from_cmd(vk_cmd);

	VkResult res = VK_SUCCESS;
	if (pool_mutex) {
		std::lock_guard<std::mutex> pool_lock(*pool_mutex);
		res = vkEndCommandBuffer(vk_cmd);
	} else {
		res = vkEndCommandBuffer(vk_cmd);
	}

	if (res != VK_SUCCESS) {
		return Error::COMMAND_SUBMISSION_FAILED;
	}

	return {};
}

Res<> VulkanDevice::command_reset(CommandBuffer cmd) {
	if (!cmd) {
		return Error::INVALID_HANDLE;
	}

	VkCommandBuffer vk_cmd = (VkCommandBuffer)cmd;
	std::shared_ptr<std::mutex> pool_mutex = _get_pool_mutex_from_cmd(vk_cmd);

	VkResult res = VK_SUCCESS;
	if (pool_mutex) {
		std::lock_guard<std::mutex> pool_lock(*pool_mutex);
		res = vkResetCommandBuffer(vk_cmd, 0);
	} else {
		res = vkResetCommandBuffer(vk_cmd, 0);
	}

	if (res != VK_SUCCESS) {
		return Error::INVALID_OPERATION;
	}

	return {};
}

Res<> VulkanDevice::command_begin_rendering(CommandBuffer cmd, const Vec2u& draw_extent,
		VectorView<RenderingAttachment> color_attachments, Image depth_attachment) {
	if (!cmd) {
		return Error::INVALID_HANDLE;
	}

	std::vector<VkRenderingAttachmentInfo> color_attachment_infos;
	for (const auto& attachment : color_attachments) {
		VulkanImage* vk_image = (VulkanImage*)attachment.image;
		if (!vk_image)
			return Error::INVALID_HANDLE;

		VkRenderingAttachmentInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		info.pNext = nullptr;
		info.imageView = vk_image->vk_image_view;
		info.imageLayout = static_cast<VkImageLayout>(attachment.layout);
		info.loadOp = static_cast<VkAttachmentLoadOp>(attachment.load_op);
		info.storeOp = static_cast<VkAttachmentStoreOp>(attachment.store_op);
		// Set clear color if any provided
		if (attachment.clear_color != COLOR_TRANSPARENT) {
			info.clearValue = { {
					attachment.clear_color.r,
					attachment.clear_color.g,
					attachment.clear_color.b,
					attachment.clear_color.a,
			} };
		}

		// Resolve image (MSAA) if provided
		if (attachment.resolve_image != GL_NULL_HANDLE) {
			VulkanImage* vk_resolve = (VulkanImage*)attachment.resolve_image;

			info.resolveMode = static_cast<VkResolveModeFlagBits>(attachment.resolve_mode);
			info.resolveImageView = vk_resolve->vk_image_view;
			info.resolveImageLayout = static_cast<VkImageLayout>(attachment.resolve_layout);
		}

		color_attachment_infos.push_back(info);
	}

	VkRenderingAttachmentInfo depth_attachment_info = {};
	if (depth_attachment) {
		VulkanImage* vk_depth_image = (VulkanImage*)depth_attachment;

		depth_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depth_attachment_info.pNext = nullptr;
		depth_attachment_info.imageView = vk_depth_image->vk_image_view;
		depth_attachment_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		depth_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depth_attachment_info.clearValue.depthStencil.depth = 1.0f;
	}

	VkRenderingInfo render_info = {};
	render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	render_info.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, { draw_extent.x, draw_extent.y } };
	render_info.layerCount = 1;
	render_info.colorAttachmentCount = static_cast<uint32_t>(color_attachment_infos.size());
	render_info.pColorAttachments = color_attachment_infos.data();
	render_info.pDepthAttachment = depth_attachment == nullptr ? nullptr : &depth_attachment_info;
	render_info.pStencilAttachment = nullptr;

	vkCmdBeginRendering((VkCommandBuffer)cmd, &render_info);

	return {};
}

Res<> VulkanDevice::command_end_rendering(CommandBuffer cmd) {
	if (!cmd) {
		return Error::INVALID_HANDLE;
	}

	vkCmdEndRendering((VkCommandBuffer)cmd);

	return {};
}

Res<> VulkanDevice::command_begin_render_pass(CommandBuffer cmd, RenderPass render_pass,
		FrameBuffer framebuffer, const Vec2u& draw_extent, Color clear_color) {
	VulkanRenderPass* render_pass_info = (VulkanRenderPass*)render_pass;
	if (!cmd || !render_pass_info) {
		return Error::INVALID_HANDLE;
	}

	std::vector<VkClearValue> clear_values(render_pass_info->attachments.size());
	for (size_t i = 0; i < render_pass_info->attachments.size(); ++i) {
		VkClearValue& clear = clear_values[i];
		const RenderPassAttachment& attachment = render_pass_info->attachments[i];

		if (attachment.is_depth_attachment) {
			clear.depthStencil = { 1.0f, 0 };
		} else {
			clear.color = { { clear_color.r, clear_color.g, clear_color.b, 1.0f } };
		}
	}

	VkRenderPassBeginInfo begin_info = {};
	begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	begin_info.renderPass = render_pass_info->vk_render_pass;
	begin_info.framebuffer = VkFramebuffer(framebuffer);
	begin_info.renderArea.offset = { 0, 0 };
	begin_info.renderArea.extent = { draw_extent.x, draw_extent.y };
	begin_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
	begin_info.pClearValues = clear_values.data();

	vkCmdBeginRenderPass((VkCommandBuffer)cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

	return {};
}

Res<> VulkanDevice::command_end_render_pass(CommandBuffer cmd) {
	if (!cmd) {
		return Error::INVALID_HANDLE;
	}

	vkCmdEndRenderPass((VkCommandBuffer)cmd);

	return {};
}

Res<> VulkanDevice::command_clear_color(CommandBuffer cmd, Image image,
		const Color& clear_color_val, ImageAspectFlags image_aspect) {
	VulkanImage* vk_image = (VulkanImage*)image;
	if (!cmd || !vk_image) {
		return Error::INVALID_HANDLE;
	}

	VkClearColorValue clear_color = {};
	static_assert(sizeof(VkClearColorValue) == sizeof(Color));
	memcpy(&clear_color.float32, &clear_color_val.r, sizeof(Color));

	VkImageSubresourceRange image_range = {};
	image_range.aspectMask = [image_aspect]() -> VkImageAspectFlags {
		switch (image_aspect) {
			case IMAGE_ASPECT_COLOR_BIT:
				return VK_IMAGE_ASPECT_COLOR_BIT;
			case IMAGE_ASPECT_DEPTH_BIT:
				return VK_IMAGE_ASPECT_DEPTH_BIT;
			case IMAGE_ASPECT_STENCIL_BIT:
				return VK_IMAGE_ASPECT_STENCIL_BIT;
			default:
				return VK_IMAGE_ASPECT_NONE;
		}
	}();
	image_range.levelCount = 1;
	image_range.layerCount = 1;

	vkCmdClearColorImage((VkCommandBuffer)cmd, vk_image->vk_image, VK_IMAGE_LAYOUT_GENERAL,
			&clear_color, 1, &image_range);

	return {};
}

Res<> VulkanDevice::command_bind_graphics_pipeline(CommandBuffer cmd, Pipeline pipeline) {
	VulkanPipeline* vk_pipeline = (VulkanPipeline*)pipeline;
	if (!cmd || !vk_pipeline) {
		return Error::INVALID_HANDLE;
	}

	vkCmdBindPipeline(
			(VkCommandBuffer)cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline->vk_pipeline);

	return {};
}

Res<> VulkanDevice::command_bind_compute_pipeline(CommandBuffer cmd, Pipeline pipeline) {
	VulkanPipeline* vk_pipeline = (VulkanPipeline*)pipeline;
	if (!cmd || !vk_pipeline) {
		return Error::INVALID_HANDLE;
	}

	vkCmdBindPipeline(
			(VkCommandBuffer)cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk_pipeline->vk_pipeline);

	return {};
}

Res<> VulkanDevice::command_bind_vertex_buffers(CommandBuffer cmd, uint32_t first_binding,
		VectorView<Buffer> vertex_buffers, VectorView<uint64_t> offsets) {
	if (!cmd) {
		return Error::INVALID_HANDLE;
	}

	if (vertex_buffers.size() != offsets.size()) {
		return Error::INVALID_ARGUMENT;
	}

	std::vector<VkBuffer> vk_buffers(vertex_buffers.size());
	for (size_t i = 0; i < vertex_buffers.size(); ++i) {
		VulkanBuffer* vk_buffer = (VulkanBuffer*)vertex_buffers[i];
		vk_buffers[i] = vk_buffer->vk_buffer;
	}

	vkCmdBindVertexBuffers((VkCommandBuffer)cmd, first_binding,
			static_cast<uint32_t>(vk_buffers.size()), vk_buffers.data(), offsets.data());
	return {};
}

Res<> VulkanDevice::command_bind_index_buffer(
		CommandBuffer cmd, Buffer index_buffer, uint64_t offset, IndexType index_type) {
	VulkanBuffer* vk_buffer = (VulkanBuffer*)index_buffer;
	if (!cmd || !vk_buffer) {
		return Error::INVALID_HANDLE;
	}

	vkCmdBindIndexBuffer((VkCommandBuffer)cmd, vk_buffer->vk_buffer, offset,
			index_type == IndexType::UINT16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);

	return {};
}

Res<> VulkanDevice::command_draw(CommandBuffer cmd, uint32_t vertex_count, uint32_t instance_count,
		uint32_t first_vertex, uint32_t first_instance) {
	if (!cmd) {
		return Error::INVALID_HANDLE;
	}

	vkCmdDraw((VkCommandBuffer)cmd, vertex_count, instance_count, first_vertex, first_instance);

	return {};
}

Res<> VulkanDevice::command_draw_indexed(CommandBuffer cmd, uint32_t index_count,
		uint32_t instance_count, uint32_t first_index, int32_t vertex_offset,
		uint32_t first_instance) {
	if (!cmd) {
		return Error::INVALID_HANDLE;
	}

	vkCmdDrawIndexed((VkCommandBuffer)cmd, index_count, instance_count, first_index, vertex_offset,
			first_instance);

	return {};
}

Res<> VulkanDevice::command_draw_indexed_indirect(
		CommandBuffer cmd, Buffer buffer, uint64_t offset, uint32_t draw_count, uint32_t stride) {
	VulkanBuffer* vk_buffer = (VulkanBuffer*)buffer;
	if (!cmd || !vk_buffer) {
		return Error::INVALID_HANDLE;
	}

	vkCmdDrawIndexedIndirect(
			(VkCommandBuffer)cmd, vk_buffer->vk_buffer, offset, draw_count, stride);

	return {};
}

Res<> VulkanDevice::command_dispatch(
		CommandBuffer cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) {
	if (!cmd) {
		return Error::INVALID_HANDLE;
	}

	vkCmdDispatch((VkCommandBuffer)cmd, group_count_x, group_count_y, group_count_z);

	return {};
}

Res<> VulkanDevice::command_bind_uniform_sets(CommandBuffer cmd, Shader shader, uint32_t first_set,
		VectorView<UniformSet> uniform_sets, PipelineType type) {
	VulkanShader* vk_shader = (VulkanShader*)shader;
	if (!cmd || !vk_shader) {
		return Error::INVALID_HANDLE;
	}

	std::vector<VkDescriptorSet> vk_sets;
	for (uint32_t i = 0; i < uniform_sets.size(); i++) {
		VulkanUniformSet* uniform_set = (VulkanUniformSet*)uniform_sets[i];
		vk_sets.push_back(uniform_set->vk_descriptor_set);
	}

	vkCmdBindDescriptorSets((VkCommandBuffer)cmd,
			type == PipelineType::GRAPHICS ? VK_PIPELINE_BIND_POINT_GRAPHICS
										   : VK_PIPELINE_BIND_POINT_COMPUTE,
			vk_shader->pipeline_layout, first_set, static_cast<uint32_t>(uniform_sets.size()),
			vk_sets.data(), 0, nullptr);

	return {};
}

Res<> VulkanDevice::command_push_constants(CommandBuffer cmd, Shader shader, uint64_t offset,
		uint32_t size, const void* push_constants) {
	VulkanShader* vk_shader = (VulkanShader*)shader;
	if (!cmd || !vk_shader) {
		return Error::INVALID_HANDLE;
	}

	vkCmdPushConstants((VkCommandBuffer)cmd, vk_shader->pipeline_layout,
			vk_shader->push_constant_stages, offset, size, push_constants);

	return {};
}

Res<> VulkanDevice::command_set_viewport(CommandBuffer cmd, const Vec2u& size) {
	if (!cmd) {
		return Error::INVALID_HANDLE;
	}

	VkViewport viewport = {
		.x = 0,
		.y = 0,
		.width = (float)size.x,
		.height = (float)size.y,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};

	vkCmdSetViewport((VkCommandBuffer)cmd, 0, 1, &viewport);

	return {};
}

Res<> VulkanDevice::command_set_scissor(CommandBuffer cmd, const Vec2u& size, const Vec2u& offset) {
	if (!cmd) {
		return Error::INVALID_HANDLE;
	}

	VkRect2D scissor = {};
	memcpy(&scissor.extent, &size, sizeof(VkExtent2D));
	memcpy(&scissor.offset, &offset, sizeof(VkExtent2D));

	vkCmdSetScissor((VkCommandBuffer)cmd, 0, 1, &scissor);
	return {};
}

Res<> VulkanDevice::command_set_depth_bias(CommandBuffer cmd, float depth_bias_constant_factor,
		float depth_bias_clamp, float depth_bias_slope_factor) {
	if (!cmd) {
		return Error::INVALID_HANDLE;
	}

	vkCmdSetDepthBias((VkCommandBuffer)cmd, depth_bias_constant_factor, depth_bias_clamp,
			depth_bias_slope_factor);

	return {};
}

Res<> VulkanDevice::command_buffer_memory_barrier(
		CommandBuffer cmd, BufferUsageFlags src_usage, BufferUsageFlags dst_usage, Buffer buffer) {
	VulkanBuffer* vk_buffer = (VulkanBuffer*)buffer;
	if (!cmd || !vk_buffer) {
		return Error::INVALID_HANDLE;
	}

	VkAccessFlags src_access = static_cast<VkAccessFlags>(src_usage);
	VkAccessFlags dst_access = static_cast<VkAccessFlags>(dst_usage);

	VkPipelineStageFlags src_stage = static_cast<VkPipelineStageFlags>(src_usage);
	VkPipelineStageFlags dst_stage = static_cast<VkPipelineStageFlags>(dst_usage);

	VkBufferMemoryBarrier buffer_barrier = {};
	buffer_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	buffer_barrier.pNext = nullptr;
	// Access Flags
	buffer_barrier.srcAccessMask = src_access;
	buffer_barrier.dstAccessMask = dst_access;
	// Resource Scope (Queue ownership transfer—usually irrelevant in a single queue family)
	buffer_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	buffer_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	// Buffer Details
	buffer_barrier.buffer = vk_buffer->vk_buffer;
	buffer_barrier.offset = 0;
	buffer_barrier.size =
			vk_buffer->allocation.size != UINT64_MAX ? vk_buffer->allocation.size : VK_WHOLE_SIZE;

	vkCmdPipelineBarrier((VkCommandBuffer)cmd, src_stage, dst_stage, 0, 0, nullptr, 1,
			&buffer_barrier, 0, nullptr);

	return {};
}

Res<> VulkanDevice::command_pipeline_barrier(CommandBuffer cmd,
		VectorView<BufferBarrier> buffer_barriers,
		VectorView<ImageBarrier> image_barriers) {
	if (!cmd) {
		return Error::INVALID_HANDLE;
	}

	VkPipelineStageFlags global_src_stages = 0;
	VkPipelineStageFlags global_dst_stages = 0;

	std::vector<VkBufferMemoryBarrier> vk_buffer_barriers;
	vk_buffer_barriers.reserve(buffer_barriers.size());

	for (const auto& barrier : buffer_barriers) {
		if (!barrier.buffer) {
			return Error::INVALID_HANDLE;
		}
		VulkanBuffer* vk_buf = (VulkanBuffer*)barrier.buffer;

		VkBufferMemoryBarrier vk_barrier = {};
		vk_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		vk_barrier.pNext = nullptr;
		vk_barrier.srcAccessMask = static_cast<VkAccessFlags>(barrier.src_access);
		vk_barrier.dstAccessMask = static_cast<VkAccessFlags>(barrier.dst_access);
		vk_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		vk_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		vk_barrier.buffer = vk_buf->vk_buffer;
		vk_barrier.offset = barrier.offset;
		vk_barrier.size = (barrier.size == ~0ULL)
				? (vk_buf->allocation.size != UINT64_MAX ? vk_buf->allocation.size : VK_WHOLE_SIZE)
				: barrier.size;

		vk_buffer_barriers.push_back(vk_barrier);

		global_src_stages |= static_cast<VkPipelineStageFlags>(barrier.src_stage);
		global_dst_stages |= static_cast<VkPipelineStageFlags>(barrier.dst_stage);
	}

	std::vector<VkImageMemoryBarrier> vk_image_barriers;
	vk_image_barriers.reserve(image_barriers.size());

	for (const auto& barrier : image_barriers) {
		if (!barrier.image) {
			return Error::INVALID_HANDLE;
		}
		VulkanImage* vk_img = (VulkanImage*)barrier.image;

		VkImageAspectFlags aspect_mask =
				(barrier.old_layout == ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
						barrier.new_layout == ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
						is_depth_format(static_cast<DataFormat>(vk_img->image_format)))
				? VK_IMAGE_ASPECT_DEPTH_BIT
				: VK_IMAGE_ASPECT_COLOR_BIT;

		VkImageMemoryBarrier vk_barrier = {};
		vk_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		vk_barrier.pNext = nullptr;
		vk_barrier.srcAccessMask = static_cast<VkAccessFlags>(barrier.src_access);
		vk_barrier.dstAccessMask = static_cast<VkAccessFlags>(barrier.dst_access);
		vk_barrier.oldLayout = static_cast<VkImageLayout>(barrier.old_layout);
		vk_barrier.newLayout = static_cast<VkImageLayout>(barrier.new_layout);
		vk_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		vk_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		vk_barrier.image = vk_img->vk_image;
		vk_barrier.subresourceRange.aspectMask = aspect_mask;
		vk_barrier.subresourceRange.baseMipLevel = barrier.base_mip_level;
		vk_barrier.subresourceRange.levelCount = barrier.level_count;
		vk_barrier.subresourceRange.baseArrayLayer = 0;
		vk_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

		vk_image_barriers.push_back(vk_barrier);

		global_src_stages |= static_cast<VkPipelineStageFlags>(barrier.src_stage);
		global_dst_stages |= static_cast<VkPipelineStageFlags>(barrier.dst_stage);
	}

	// If no barriers were provided, or stages are 0, default to TOP_OF_PIPE/BOTTOM_OF_PIPE or do nothing.
	if (global_src_stages == 0) {
		global_src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	}
	if (global_dst_stages == 0) {
		global_dst_stages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	}

	vkCmdPipelineBarrier((VkCommandBuffer)cmd,
			global_src_stages,
			global_dst_stages,
			0,
			0, nullptr,
			static_cast<uint32_t>(vk_buffer_barriers.size()), vk_buffer_barriers.data(),
			static_cast<uint32_t>(vk_image_barriers.size()), vk_image_barriers.data());

	return {};
}


Res<> VulkanDevice::command_copy_buffer(CommandBuffer cmd, Buffer src_buffer, Buffer dst_buffer,
		VectorView<BufferCopyRegion> regions) {
	VulkanBuffer* vk_src = (VulkanBuffer*)src_buffer;
	VulkanBuffer* vk_dst = (VulkanBuffer*)dst_buffer;
	if (!cmd || !vk_src || !vk_dst) {
		return Error::INVALID_HANDLE;
	}

	std::vector<VkBufferCopy> vk_regions(regions.size());
	for (uint32_t i = 0; i < regions.size(); i++) {
		VkBufferCopy& copy = vk_regions[i];
		memcpy(&copy, &regions[i], sizeof(VkBufferCopy));
	}

	vkCmdCopyBuffer((VkCommandBuffer)cmd, vk_src->vk_buffer, vk_dst->vk_buffer,
			static_cast<uint32_t>(regions.size()), vk_regions.data());

	return {};
}

Res<> VulkanDevice::command_copy_buffer_to_image(CommandBuffer cmd, Buffer src_buffer,
		Image dst_image, VectorView<BufferImageCopyRegion> regions) {
	VulkanBuffer* vk_src = (VulkanBuffer*)src_buffer;
	VulkanImage* vk_dst = (VulkanImage*)dst_image;
	if (!cmd || !vk_src || !vk_dst) {
		return Error::INVALID_HANDLE;
	}

	std::vector<VkBufferImageCopy> vk_regions(regions.size());
	for (uint32_t i = 0; i < regions.size(); i++) {
		VkBufferImageCopy& copy = vk_regions[i];
		memcpy(&copy, &regions[i], sizeof(VkBufferImageCopy));
	}

	vkCmdCopyBufferToImage((VkCommandBuffer)cmd, vk_src->vk_buffer, vk_dst->vk_image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(regions.size()),
			vk_regions.data());

	return {};
}

Res<> VulkanDevice::command_copy_image_to_image(CommandBuffer cmd, Image src_image, Image dst_image,
		const Vec2u& src_extent, const Vec2u& dst_extent, uint32_t src_mip_level,
		uint32_t dst_mip_level) {
	if (!cmd || !src_image || !dst_image) {
		return Error::INVALID_HANDLE;
	}

	VkImageBlit2 blit_region = {};
	blit_region.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;

	blit_region.srcOffsets[1].x = src_extent.x;
	blit_region.srcOffsets[1].y = src_extent.y;
	blit_region.srcOffsets[1].z = 1;

	blit_region.dstOffsets[1].x = dst_extent.x;
	blit_region.dstOffsets[1].y = dst_extent.y;
	blit_region.dstOffsets[1].z = 1;

	blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit_region.srcSubresource.baseArrayLayer = 0;
	blit_region.srcSubresource.layerCount = 1;
	blit_region.srcSubresource.mipLevel = src_mip_level;

	blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit_region.dstSubresource.baseArrayLayer = 0;
	blit_region.dstSubresource.layerCount = 1;
	blit_region.dstSubresource.mipLevel = dst_mip_level;

	VulkanImage* vk_src = (VulkanImage*)src_image;
	VulkanImage* vk_dst = (VulkanImage*)dst_image;

	VkBlitImageInfo2 blit_info = {};
	blit_info.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
	blit_info.srcImage = vk_src->vk_image;
	blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blit_info.dstImage = vk_dst->vk_image;
	blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blit_info.regionCount = 1;
	blit_info.pRegions = &blit_region;
	blit_info.filter = VK_FILTER_LINEAR;

	vkCmdBlitImage2((VkCommandBuffer)cmd, &blit_info);

	return {};
}

Res<> VulkanDevice::command_transition_image(CommandBuffer cmd, Image image,
		ImageLayout current_layout, ImageLayout new_layout, uint32_t base_mip_level,
		uint32_t level_count) {
	if (!cmd || !image) {
		return Error::INVALID_HANDLE;
	}

	VkImageAspectFlags aspect_mask =
			(current_layout == ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
					new_layout == ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
			? VK_IMAGE_ASPECT_DEPTH_BIT
			: VK_IMAGE_ASPECT_COLOR_BIT;

	VkImageLayout vk_current_layout = static_cast<VkImageLayout>(current_layout);
	VkImageLayout vk_new_layout = static_cast<VkImageLayout>(new_layout);

	VulkanImage* vk_image = (VulkanImage*)image;

	VkImageSubresourceRange sub_image = {};
	sub_image.aspectMask = aspect_mask;
	sub_image.baseMipLevel = base_mip_level;
	sub_image.levelCount = level_count;
	sub_image.baseArrayLayer = 0;
	sub_image.layerCount = VK_REMAINING_ARRAY_LAYERS;

	VkImageMemoryBarrier2 image_barrier = {};
	image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	image_barrier.pNext = nullptr;
	image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	image_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	image_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
	image_barrier.oldLayout = vk_current_layout;
	image_barrier.newLayout = vk_new_layout;
	image_barrier.image = vk_image->vk_image;
	image_barrier.subresourceRange = sub_image;

	VkDependencyInfo dep_info = {};
	dep_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dep_info.pNext = nullptr;
	dep_info.imageMemoryBarrierCount = 1;
	dep_info.pImageMemoryBarriers = &image_barrier;

	vkCmdPipelineBarrier2((VkCommandBuffer)cmd, &dep_info);

	return {};
}

} //namespace gpukitkit
