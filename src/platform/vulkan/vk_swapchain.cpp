#include "platform/vulkan/vk_backend.h"

#include <vulkan/vulkan_core.h>

namespace gl {

void VulkanRenderBackend::_swapchain_release(VulkanSwapchain* p_swapchain) {
	if (!p_swapchain) {
		return;
	}

	// Destroy image views
	for (auto& image : p_swapchain->images) {
		if (image.vk_image_view != VK_NULL_HANDLE) {
			vkDestroyImageView(device, image.vk_image_view, nullptr);
			image.vk_image_view = VK_NULL_HANDLE;
		}
	}
	p_swapchain->images.clear();

	// Destroy the swapchain handle
	if (p_swapchain->vk_swapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(device, p_swapchain->vk_swapchain, nullptr);
		p_swapchain->vk_swapchain = VK_NULL_HANDLE;
	}

	p_swapchain->initialized = false;
	p_swapchain->image_index = UINT32_MAX;
}

Swapchain VulkanRenderBackend::swapchain_create() {
	VulkanSwapchain* swapchain = new VulkanSwapchain();
	swapchain->format = VK_FORMAT_R8G8B8A8_UNORM; // preferred but not guaranteed
	swapchain->color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	swapchain->vk_swapchain = VK_NULL_HANDLE;
	swapchain->initialized = false;

	return Swapchain(swapchain);
}

void VulkanRenderBackend::swapchain_resize(
		CommandQueue p_cmd_queue, Swapchain p_swapchain, Vec2u p_size, bool p_vsync) {
	if (!surface) {
		GL_LOG_WARNING("[VULKAN] Headless mode: skipping swapchain resize.");
		return;
	}

	if (!p_swapchain) {
		GL_LOG_ERROR("[VULKAN] Unable to resize null swapchain!");
		return;
	}

	VulkanSwapchain* swapchain = (VulkanSwapchain*)p_swapchain;

	vkDeviceWaitIdle(device);

	// Query Surface Capabilities
	VkSurfaceCapabilitiesKHR capabilities;
	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities) !=
			VK_SUCCESS) {
		GL_ASSERT(false, "[VULKAN] Failed to query surface capabilities.");
		return;
	}

	// Determine Extent
	VkExtent2D extent;
	if (capabilities.currentExtent.width != UINT32_MAX) {
		extent = capabilities.currentExtent;
	} else {
		extent.width = std::clamp(
				p_size.x, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		extent.height = std::clamp(
				p_size.y, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	}

	// Determine Image Count (Min + 1 for Triple Buffering usually, clamped to max)
	uint32_t image_count = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
		image_count = capabilities.maxImageCount;
	}

	// Select Surface Format
	uint32_t format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);
	std::vector<VkSurfaceFormatKHR> formats(format_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, formats.data());

	VkSurfaceFormatKHR selected_format = formats[0];
	for (const auto& available_format : formats) {
		if (available_format.format == VK_FORMAT_R8G8B8A8_UNORM &&
				available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			selected_format = available_format;
			break;
		}
		// Fallback to BGR if RGB not found
		if (available_format.format == VK_FORMAT_B8G8R8A8_UNORM &&
				available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			selected_format = available_format;
		}
	}

	// Select Present Mode
	uint32_t mode_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &mode_count, nullptr);
	std::vector<VkPresentModeKHR> present_modes(mode_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(
			physical_device, surface, &mode_count, present_modes.data());

	VkPresentModeKHR present_mode =
			VK_PRESENT_MODE_FIFO_KHR; // VSync on (guaranteed to be available)

	if (!p_vsync) {
		// Try Mailbox (Triple Buffering / uncapped)
		bool mailbox_found = false;
		for (const auto& mode : present_modes) {
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
				present_mode = mode;
				mailbox_found = true;
				break;
			}
		}
		// Fallback to Immediate if Mailbox not supported
		if (!mailbox_found) {
			for (const auto& mode : present_modes) {
				if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
					present_mode = mode;
					break;
				}
			}
		}
	}

	// Create the Swapchain
	VkSwapchainCreateInfoKHR create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = surface;
	create_info.minImageCount = image_count;
	create_info.imageFormat = selected_format.format;
	create_info.imageColorSpace = selected_format.colorSpace;
	create_info.imageExtent = extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	// Handle Queue Families (Graphics vs Present)
	uint32_t queue_family_indices[] = { graphics_queue.queue_family, present_queue.queue_family };
	if (graphics_queue.queue_family != present_queue.queue_family) {
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queue_family_indices;
	} else {
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	create_info.preTransform = capabilities.currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = present_mode;
	create_info.clipped = VK_TRUE;

	// Smooth transition: Provide the old swapchain if it exists
	VkSwapchainKHR old_swapchain_handle = swapchain->vk_swapchain;
	create_info.oldSwapchain = old_swapchain_handle;

	VkSwapchainKHR new_vk_swapchain;
	if (vkCreateSwapchainKHR(device, &create_info, nullptr, &new_vk_swapchain) != VK_SUCCESS) {
		GL_ASSERT(false, "[VULKAN] Failed to create swapchain!");
		return;
	}

	// Clean up the old swapchain resources
	// The create info used the old handle, now we destroy the old wrapper and handle.
	if (swapchain->initialized) {
		_swapchain_release(swapchain);
	}

	// Update the wrapper with new data
	swapchain->vk_swapchain = new_vk_swapchain;
	swapchain->format = selected_format.format;
	swapchain->color_space = selected_format.colorSpace;
	swapchain->extent = extent;

	// Retrieve Images
	vkGetSwapchainImagesKHR(device, swapchain->vk_swapchain, &image_count, nullptr);
	std::vector<VkImage> raw_images(image_count);
	vkGetSwapchainImagesKHR(device, swapchain->vk_swapchain, &image_count, raw_images.data());

	// Create Image Views
	swapchain->images.resize(image_count);
	for (size_t i = 0; i < image_count; i++) {
		swapchain->images[i].vk_image = raw_images[i];
		swapchain->images[i].image_format = selected_format.format;
		swapchain->images[i].image_extent = VkExtent3D{ extent.width, extent.height, 1 };

		VkImageViewCreateInfo view_info = {};
		view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_info.image = raw_images[i];
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.format = selected_format.format;

		// Default Component Mapping
		view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.levelCount = 1;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device, &view_info, nullptr, &swapchain->images[i].vk_image_view) !=
				VK_SUCCESS) {
			GL_ASSERT(false, "[VULKAN] Failed to create swapchain image view!");
		}
	}

	swapchain->initialized = true;
	GL_LOG_TRACE("[VULKAN] Swapchain resized to {}x{}", extent.width, extent.height);
}

size_t VulkanRenderBackend::swapchain_get_image_count(Swapchain p_swapchain) {
	VulkanSwapchain* swapchain = (VulkanSwapchain*)p_swapchain;
	return swapchain->images.size();
}

std::vector<Image> VulkanRenderBackend::swapchain_get_images(Swapchain p_swapchain) {
	VulkanSwapchain* swapchain = (VulkanSwapchain*)p_swapchain;

	std::vector<Image> images(swapchain->images.size());
	for (size_t i = 0; i < swapchain->images.size(); i++) {
		images[i] = Image(&swapchain->images[i]);
	}

	return images;
}

Result<Image, SwapchainAcquireError> VulkanRenderBackend::swapchain_acquire_image(
		Swapchain p_swapchain, Semaphore p_semaphore, uint32_t* o_image_index) {
	VulkanSwapchain* swapchain = (VulkanSwapchain*)p_swapchain;

	const VkResult res = vkAcquireNextImageKHR(device, swapchain->vk_swapchain, UINT64_MAX,
			(VkSemaphore)p_semaphore, VK_NULL_HANDLE, &swapchain->image_index);

	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
		return make_err<Image>(SwapchainAcquireError::OUT_OF_DATE);
	} else if (res != VK_SUCCESS) {
		return make_err<Image>(SwapchainAcquireError::ERROR);
	}

	if (o_image_index) {
		*o_image_index = swapchain->image_index;
	}

	return Image(&swapchain->images[swapchain->image_index]);
}

Vec2u VulkanRenderBackend::swapchain_get_extent(Swapchain p_swapchain) {
	GL_ASSERT(p_swapchain != nullptr);

	VulkanSwapchain* swapchain = (VulkanSwapchain*)p_swapchain;

	Vec2u extent;
	static_assert(sizeof(Vec2u) == sizeof(VkExtent2D));
	memcpy(&extent, &swapchain->extent, sizeof(VkExtent2D));

	return extent;
}

DataFormat VulkanRenderBackend::swapchain_get_format(Swapchain p_swapchain) {
	GL_ASSERT(p_swapchain != nullptr);

	VulkanSwapchain* swapchain = (VulkanSwapchain*)p_swapchain;
	return static_cast<DataFormat>(swapchain->format);
}

void VulkanRenderBackend::swapchain_free(Swapchain p_swapchain) {
	GL_ASSERT(p_swapchain != nullptr);

	VulkanSwapchain* swapchain = (VulkanSwapchain*)p_swapchain;
	_swapchain_release(swapchain);

	delete swapchain;
}

} //namespace gl