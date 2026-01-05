#include "platform/vulkan/vk_device.h"

namespace gl {

Fence VulkanDevice::fence_create(bool create_signaled) {
	VkFenceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	if (create_signaled) {
		create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // signal on create
	}

	VkFence vk_fence = VK_NULL_HANDLE;
	VK_CHECK_RET(vkCreateFence(_device, &create_info, nullptr, &vk_fence), GL_NULL_HANDLE);

	return Fence(vk_fence);
}

Res<> VulkanDevice::fence_free(Fence fence) {
	if (!fence) {
		return {};
	}

	vkDestroyFence(_device, (VkFence)fence, nullptr);

	return {};
}

Res<> VulkanDevice::fence_wait(Fence fence) {
	VkFence vk_fence = (VkFence)fence;
	if (!vk_fence) {
		return Error::INVALID_HANDLE;
	}

	// Waiting with UINT64_MAX means we shouldn't timeout, but if we do (or device lost), return
	// error.
	VK_CHECK_RET(vkWaitForFences(_device, 1, &vk_fence, VK_TRUE, UINT64_MAX), Error::FENCE_TIMEOUT);

	return {};
}

Res<> VulkanDevice::fence_reset(Fence fence) {
	VkFence vk_fence = (VkFence)fence;
	if (!vk_fence) {
		return Error::INVALID_HANDLE;
	}

	VK_CHECK_RET(vkResetFences(_device, 1, &vk_fence), Error::INITIALIZATION_FAILED);

	return {};
}

Semaphore VulkanDevice::semaphore_create() {
	VkSemaphoreCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkSemaphore vk_semaphore = VK_NULL_HANDLE;
	VK_CHECK_RET(vkCreateSemaphore(_device, &create_info, nullptr, &vk_semaphore), GL_NULL_HANDLE);

	return Semaphore(vk_semaphore);
}

Res<> VulkanDevice::semaphore_free(Semaphore semaphore) {
	if (!semaphore) {
		return {};
	}

	vkDestroySemaphore(_device, (VkSemaphore)semaphore, nullptr);

	return {};
}

} //namespace gl
