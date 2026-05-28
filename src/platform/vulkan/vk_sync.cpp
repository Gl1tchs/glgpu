#include "platform/vulkan/vk_device.h"

namespace gpukit {

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

Res<Semaphore> VulkanDevice::timeline_semaphore_create(uint64_t initial_value) {
	VkSemaphoreTypeCreateInfo type_info = {};
	type_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	type_info.initialValue = initial_value;

	VkSemaphoreCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	create_info.pNext = &type_info;

	VkSemaphore vk_semaphore = VK_NULL_HANDLE;
	VK_CHECK_RET(vkCreateSemaphore(_device, &create_info, nullptr, &vk_semaphore),
			make_err<Semaphore>(Error::INITIALIZATION_FAILED));

	return Semaphore(vk_semaphore);
}

Res<> VulkanDevice::semaphore_free(Semaphore semaphore) {
	if (!semaphore) {
		return {};
	}

	vkDestroySemaphore(_device, (VkSemaphore)semaphore, nullptr);

	return {};
}

Res<> VulkanDevice::semaphore_signal(Semaphore semaphore, uint64_t value) {
	if (!semaphore) {
		return Error::INVALID_HANDLE;
	}

	VkSemaphoreSignalInfo signal_info = {};
	signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
	signal_info.semaphore = (VkSemaphore)semaphore;
	signal_info.value = value;

	VK_CHECK_RET(vkSignalSemaphore(_device, &signal_info), Error::INVALID_OPERATION);

	return {};
}

Res<> VulkanDevice::semaphore_wait(Semaphore semaphore, uint64_t value, uint64_t timeout_ns) {
	if (!semaphore) {
		return Error::INVALID_HANDLE;
	}

	VkSemaphore vk_sem = (VkSemaphore)semaphore;

	VkSemaphoreWaitInfo wait_info = {};
	wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	wait_info.semaphoreCount = 1;
	wait_info.pSemaphores = &vk_sem;
	wait_info.pValues = &value;

	VK_CHECK_RET(vkWaitSemaphores(_device, &wait_info, timeout_ns), Error::FENCE_TIMEOUT);

	return {};
}

Res<uint64_t> VulkanDevice::semaphore_get_value(Semaphore semaphore) {
	if (!semaphore) {
		return make_err<uint64_t>(Error::INVALID_HANDLE);
	}

	uint64_t value = 0;
	VK_CHECK_RET(vkGetSemaphoreCounterValue(_device, (VkSemaphore)semaphore, &value),
			make_err<uint64_t>(Error::INVALID_OPERATION));

	return value;
}

} //namespace gpukit
