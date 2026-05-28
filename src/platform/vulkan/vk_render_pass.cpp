#include "platform/vulkan/vk_device.h"

#include "platform/vulkan/vk_common.h"

namespace gpukit {

Res<RenderPass> VulkanDevice::render_pass_create(
		VectorView<RenderPassAttachment> attachments, VectorView<SubpassInfo> subpasses) {
	std::vector<VkAttachmentDescription> vk_attachments;
	for (const auto& attachment : attachments) {
		VkAttachmentDescription vk_attachment = {};
		vk_attachment.format = static_cast<VkFormat>(attachment.format);
		vk_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		vk_attachment.loadOp = static_cast<VkAttachmentLoadOp>(attachment.load_op);
		vk_attachment.storeOp = static_cast<VkAttachmentStoreOp>(attachment.store_op);
		vk_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		vk_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		vk_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if (attachment.final_layout == ImageLayout::UNDEFINED) {
			vk_attachment.finalLayout = static_cast<VkImageLayout>(attachment.final_layout);
		} else {
			vk_attachment.finalLayout = attachment.is_depth_attachment
					? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
					: VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		}

		vk_attachments.push_back(vk_attachment);
	}

	std::vector<std::vector<VkAttachmentReference>> vk_color_attachment_refs;
	std::vector<std::vector<VkAttachmentReference>> vk_input_attachment_refs;
	// We need a stable container for depth refs because we store pointers to them
	std::vector<VkAttachmentReference> vk_depth_attachment_refs;
	// We reserve enough space to avoid reallocation invalidating pointers
	vk_depth_attachment_refs.reserve(subpasses.size());

	std::vector<VkSubpassDescription> vk_subpasses;
	for (const auto& subpass : subpasses) {
		std::vector<VkAttachmentReference> color_refs;
		std::vector<VkAttachmentReference> input_refs;
		std::optional<VkAttachmentReference> depth_ref;

		for (const auto& attachment : subpass.attachments) {
			VkAttachmentReference ref = {};
			ref.attachment = attachment.attachment_index;

			switch (attachment.type) {
				case SUBPASS_ATTACHMENT_COLOR:
					ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					color_refs.push_back(ref);
					break;
				case SUBPASS_ATTACHMENT_INPUT:
					ref.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					input_refs.push_back(ref);
					break;
				case SUBPASS_ATTACHMENT_DEPTH_STENCIL:
					ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					depth_ref = ref;
					break;
			}
		}

		// Push ref containers to keep them alive
		vk_color_attachment_refs.push_back(std::move(color_refs));
		vk_input_attachment_refs.push_back(std::move(input_refs));

		VkSubpassDescription vk_subpass = {};
		vk_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		vk_subpass.colorAttachmentCount =
				static_cast<uint32_t>(vk_color_attachment_refs.back().size());
		vk_subpass.pColorAttachments = vk_color_attachment_refs.back().data();
		vk_subpass.inputAttachmentCount =
				static_cast<uint32_t>(vk_input_attachment_refs.back().size());
		vk_subpass.pInputAttachments = vk_input_attachment_refs.back().data();

		if (depth_ref) {
			vk_depth_attachment_refs.push_back(*depth_ref);
			vk_subpass.pDepthStencilAttachment = &vk_depth_attachment_refs.back();
		}

		vk_subpasses.push_back(vk_subpass);
	}

	std::vector<VkSubpassDependency> vk_dependencies;
	for (uint32_t i = 0; i + 1 < vk_subpasses.size(); ++i) {
		VkSubpassDependency dep = {};
		dep.srcSubpass = i;
		dep.dstSubpass = i + 1;
		dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dep.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
		dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		vk_dependencies.push_back(dep);
	}

	VkRenderPassCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	create_info.attachmentCount = static_cast<uint32_t>(vk_attachments.size());
	create_info.pAttachments = vk_attachments.data();
	create_info.subpassCount = static_cast<uint32_t>(vk_subpasses.size());
	create_info.pSubpasses = vk_subpasses.data();
	create_info.dependencyCount = static_cast<uint32_t>(vk_dependencies.size());
	create_info.pDependencies = vk_dependencies.data();

	VkRenderPass vk_render_pass = VK_NULL_HANDLE;
	VK_CHECK_RET(vkCreateRenderPass(_device, &create_info, nullptr, &vk_render_pass),
			make_err<RenderPass>(Error::INITIALIZATION_FAILED));

	// Bookkeeping
	VulkanRenderPass* render_pass_info =
			VersatileResource::allocate<VulkanRenderPass>(_resources_allocator);
	render_pass_info->vk_render_pass = vk_render_pass;

	// Copy attachments
	render_pass_info->attachments =
			std::vector<RenderPassAttachment>(attachments.begin(), attachments.end());

	return RenderPass(render_pass_info);
}

Res<> VulkanDevice::render_pass_destroy(RenderPass render_pass) {
	if (!render_pass) {
		return {};
	}

	VulkanRenderPass* render_pass_info = (VulkanRenderPass*)render_pass;

	vkDestroyRenderPass(_device, render_pass_info->vk_render_pass, nullptr);

	render_pass_info->~VulkanRenderPass();

	VersatileResource::free(_resources_allocator, render_pass_info);

	return {};
}

Res<FrameBuffer> VulkanDevice::frame_buffer_create(
		RenderPass render_pass, VectorView<Image> attachments, const Vec2u& extent) {
	if (!render_pass) {
		return make_err<FrameBuffer>(Error::INVALID_HANDLE);
	}

	VulkanRenderPass* render_pass_info = (VulkanRenderPass*)render_pass;

	std::vector<VkImageView> vk_attachments;
	for (const auto& attachment : attachments) {
		if (!attachment)
			return make_err<FrameBuffer>(Error::INVALID_HANDLE);
		VulkanImage* vk_image = (VulkanImage*)attachment;
		vk_attachments.push_back(vk_image->vk_image_view);
	}

	VkFramebufferCreateInfo frame_buffer_info = {};
	frame_buffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frame_buffer_info.renderPass = render_pass_info->vk_render_pass;
	frame_buffer_info.attachmentCount = static_cast<uint32_t>(vk_attachments.size());
	frame_buffer_info.pAttachments = vk_attachments.data();
	frame_buffer_info.width = extent.x;
	frame_buffer_info.height = extent.y;
	frame_buffer_info.layers = 1;

	VkFramebuffer frame_buffer = VK_NULL_HANDLE;
	VK_CHECK_RET(vkCreateFramebuffer(_device, &frame_buffer_info, nullptr, &frame_buffer),
			make_err<FrameBuffer>(Error::INITIALIZATION_FAILED));

	return FrameBuffer(frame_buffer);
}

Res<> VulkanDevice::frame_buffer_destroy(FrameBuffer frame_buffer) {
	if (!frame_buffer) {
		return {};
	}

	vkDestroyFramebuffer(_device, (VkFramebuffer)frame_buffer, nullptr);

	return {};
}

} //namespace gpukit
