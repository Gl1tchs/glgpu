#include "platform/vulkan/vk_device.h"

#include "platform/vulkan/vk_common.h"

namespace gl {

void VulkanDevice::_swapchain_release(VulkanSwapchain* swapchain) {
	if (!swapchain) {
		return;
	}

	// Destroy image views
	for (auto& image : swapchain->images) {
		if (image.vk_image_view != VK_NULL_HANDLE) {
			vkDestroyImageView(_device, image.vk_image_view, nullptr);
			image.vk_image_view = VK_NULL_HANDLE;
		}
	}
	swapchain->images.clear();

	// Destroy the swapchain handle
	if (swapchain->vk_swapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(_device, swapchain->vk_swapchain, nullptr);
		swapchain->vk_swapchain = VK_NULL_HANDLE;
	}

	swapchain->initialized = false;
	swapchain->image_index = UINT32_MAX;
}

Res<Swapchain> VulkanDevice::swapchain_create() {
	VulkanSwapchain* swapchain = new VulkanSwapchain();
	if (!swapchain) {
		return make_err<Swapchain>(Error::OUT_OF_HOST_MEMORY);
	}

	swapchain->format = VK_FORMAT_R8G8B8A8_UNORM;
	swapchain->color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	swapchain->vk_swapchain = VK_NULL_HANDLE;
	swapchain->initialized = false;

	return Swapchain(swapchain);
}

Res<> VulkanDevice::swapchain_resize(
		CommandQueue cmd_queue, Swapchain swapchain_handle, Vec2u size, bool vsync) {
	if (!_surface) {
		GL_LOG_WARNING("[VULKAN] Headless mode or no surface: skipping swapchain resize.");
		return Error::SURFACE_LOST;
	}

	if (!swapchain_handle) {
		return Error::INVALID_HANDLE;
	}

	VulkanSwapchain* swapchain = (VulkanSwapchain*)swapchain_handle;

	vkDeviceWaitIdle(_device);

	// Query Surface Capabilities
	VkSurfaceCapabilitiesKHR capabilities;
	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physical_device, _surface, &capabilities) !=
			VK_SUCCESS) {
		return Error::SURFACE_LOST;
	}

	// Determine Extent
	VkExtent2D extent;
	if (capabilities.currentExtent.width != UINT32_MAX) {
		extent = capabilities.currentExtent;
	} else {
		extent.width = std::clamp(
				size.x, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		extent.height = std::clamp(
				size.y, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	}

	if (extent.width == 0 || extent.height == 0) {
		// Window is minimized, not an error per se but we can't create a swapchain
		return Error::SWAPCHAIN_SUBOPTIMAL;
	}

	// Determine Image Count
	uint32_t image_count = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
		image_count = capabilities.maxImageCount;
	}

	// Select Surface Format
	uint32_t format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(_physical_device, _surface, &format_count, nullptr);
	std::vector<VkSurfaceFormatKHR> formats(format_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(_physical_device, _surface, &format_count, formats.data());

	if (formats.empty()) {
		return Error::SURFACE_LOST;
	}

	VkSurfaceFormatKHR selected_format = formats[0];
	for (const auto& available_format : formats) {
		if (available_format.format == VK_FORMAT_R8G8B8A8_UNORM &&
				available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			selected_format = available_format;
			break;
		}
		if (available_format.format == VK_FORMAT_B8G8R8A8_UNORM &&
				available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			selected_format = available_format;
		}
	}

	// Select Present Mode
	uint32_t mode_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(_physical_device, _surface, &mode_count, nullptr);
	std::vector<VkPresentModeKHR> present_modes(mode_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(
			_physical_device, _surface, &mode_count, present_modes.data());

	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR; // VSync on

	if (!vsync) {
		bool mailbox_found = false;
		for (const auto& mode : present_modes) {
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
				present_mode = mode;
				mailbox_found = true;
				break;
			}
		}
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
	create_info.surface = _surface;
	create_info.minImageCount = image_count;
	create_info.imageFormat = selected_format.format;
	create_info.imageColorSpace = selected_format.colorSpace;
	create_info.imageExtent = extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	uint32_t queue_family_indices[] = { _graphics_queue.queue_family, _present_queue.queue_family };
	if (_graphics_queue.queue_family != _present_queue.queue_family) {
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
	create_info.oldSwapchain = swapchain->vk_swapchain;

	VkSwapchainKHR new_vk_swapchain;
	VK_CHECK_RET(vkCreateSwapchainKHR(_device, &create_info, nullptr, &new_vk_swapchain),
			Error::INITIALIZATION_FAILED);

	// Release old resources (handles only, not the wrapper)
	if (swapchain->initialized) {
		_swapchain_release(swapchain);
	}

	swapchain->vk_swapchain = new_vk_swapchain;
	swapchain->format = selected_format.format;
	swapchain->color_space = selected_format.colorSpace;
	swapchain->extent = extent;

	// Retrieve Images
	vkGetSwapchainImagesKHR(_device, swapchain->vk_swapchain, &image_count, nullptr);
	std::vector<VkImage> raw_images(image_count);
	vkGetSwapchainImagesKHR(_device, swapchain->vk_swapchain, &image_count, raw_images.data());

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
		view_info.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.levelCount = 1;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.layerCount = 1;

		VK_CHECK_RET(vkCreateImageView(
							 _device, &view_info, nullptr, &swapchain->images[i].vk_image_view),
				Error::INITIALIZATION_FAILED);
	}

	swapchain->initialized = true;

	return {};
}

Res<size_t> VulkanDevice::swapchain_get_image_count(Swapchain swapchain) {
	VulkanSwapchain* vk_swapchain = (VulkanSwapchain*)swapchain;
	if (!vk_swapchain) {
		return make_err<size_t>(Error::INVALID_HANDLE);
	}

	return vk_swapchain->images.size();
}

Res<std::vector<Image>> VulkanDevice::swapchain_get_images(Swapchain swapchain) {
	VulkanSwapchain* vk_swapchain = (VulkanSwapchain*)swapchain;
	if (!vk_swapchain) {
		return make_err<std::vector<Image>>(Error::INVALID_HANDLE);
	}

	std::vector<Image> images;
	images.reserve(vk_swapchain->images.size());
	for (size_t i = 0; i < vk_swapchain->images.size(); i++) {
		images.push_back(Image(&vk_swapchain->images[i]));
	}

	return images;
}

Res<Image> VulkanDevice::swapchain_acquire_image(
		Swapchain swapchain, Semaphore semaphore, uint32_t* o_image_index) {
	VulkanSwapchain* vk_swapchain = (VulkanSwapchain*)swapchain;
	if (!vk_swapchain) {
		return make_err<Image>(Error::INVALID_HANDLE);
	}

	VkResult res = vkAcquireNextImageKHR(_device, vk_swapchain->vk_swapchain, UINT64_MAX,
			(VkSemaphore)semaphore, VK_NULL_HANDLE, &vk_swapchain->image_index);

	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
		return make_err<Image>(Error::SWAPCHAIN_OUT_OF_DATE);
	} else if (res != VK_SUCCESS) {
		return make_err<Image>(Error::IMAGE_ACQUIRE_FAILED);
	}

	if (o_image_index) {
		*o_image_index = vk_swapchain->image_index;
	}

	return Image(&vk_swapchain->images[vk_swapchain->image_index]);
}

Res<Vec2u> VulkanDevice::swapchain_get_extent(Swapchain swapchain) {
	VulkanSwapchain* vk_swapchain = (VulkanSwapchain*)swapchain;
	if (!vk_swapchain) {
		return make_err<Vec2u>(Error::INVALID_HANDLE);
	}

	return Vec2u{ vk_swapchain->extent.width, vk_swapchain->extent.height };
}

Res<DataFormat> VulkanDevice::swapchain_get_format(Swapchain swapchain) {
	VulkanSwapchain* vk_swapchain = (VulkanSwapchain*)swapchain;
	if (!vk_swapchain) {
		return make_err<DataFormat>(Error::INVALID_HANDLE);
	}

	return static_cast<DataFormat>(vk_swapchain->format);
}

Res<> VulkanDevice::swapchain_free(Swapchain swapchain) {
	VulkanSwapchain* vk_swapchain = (VulkanSwapchain*)swapchain;
	if (!vk_swapchain) {
		return {};
	}

	_swapchain_release(vk_swapchain);
	delete vk_swapchain;

	return {};
}

} //namespace gl
