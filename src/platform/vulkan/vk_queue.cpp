#include "platform/vulkan/vk_device.h"

namespace gpukit {

Res<CommandQueue> VulkanDevice::queue_get(QueueType type) {
	VulkanQueue* queue = nullptr;
	switch (type) {
		case QueueType::GRAPHICS:
			queue = &_graphics_queue;
			break;
		case QueueType::PRESENT:
			queue = &_present_queue;
			break;
		case QueueType::TRANSFER:
			queue = &_transfer_queue;
			break;
		case QueueType::COMPUTE:
			queue = &_compute_queue;
			break;
		default:
			queue = &_graphics_queue;
			break;
	}

	if (!queue) {
		return make_err<CommandQueue>(Error::QUEUE_FAMILY_NOT_FOUND);
	}

	return CommandQueue(queue);
}

Res<> VulkanDevice::queue_submit(CommandQueue queue, CommandBuffer cmd, Fence fence,
		Semaphore wait_semaphore, Semaphore signal_semaphore) {
	if (!queue || !cmd) {
		return Error::INVALID_HANDLE;
	}

	VkCommandBufferSubmitInfo cmd_info = {};
	cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmd_info.pNext = nullptr;
	cmd_info.commandBuffer = (VkCommandBuffer)cmd;
	cmd_info.deviceMask = 0;

	VkSemaphoreSubmitInfo wait_semaphore_info = {};
	if (wait_semaphore) {
		wait_semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		wait_semaphore_info.semaphore = (VkSemaphore)wait_semaphore;
		wait_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		wait_semaphore_info.deviceIndex = 0;
		wait_semaphore_info.value = 1;
	}

	VkSemaphoreSubmitInfo signal_semaphore_info = {};
	if (signal_semaphore) {
		signal_semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		signal_semaphore_info.semaphore = (VkSemaphore)signal_semaphore;
		signal_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
		signal_semaphore_info.deviceIndex = 0;
		signal_semaphore_info.value = 1;
	}

	VkSubmitInfo2 submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submit_info.pNext = nullptr;

	submit_info.commandBufferInfoCount = 1;
	submit_info.pCommandBufferInfos = &cmd_info;

	submit_info.waitSemaphoreInfoCount = wait_semaphore == nullptr ? 0 : 1;
	submit_info.pWaitSemaphoreInfos = wait_semaphore ? &wait_semaphore_info : nullptr;

	submit_info.signalSemaphoreInfoCount = signal_semaphore == nullptr ? 0 : 1;
	submit_info.pSignalSemaphoreInfos = signal_semaphore ? &signal_semaphore_info : nullptr;

	VulkanQueue* vk_queue = (VulkanQueue*)queue;

	if (vk_queue->mutex) {
		// Lock queue for thread safe access
		std::lock_guard<std::mutex> lock(*vk_queue->mutex);
		VK_CHECK_RET(vkQueueSubmit2(vk_queue->queue, 1, &submit_info, (VkFence)fence),
				Error::COMMAND_SUBMISSION_FAILED);
	} else {
		VK_CHECK_RET(vkQueueSubmit2(vk_queue->queue, 1, &submit_info, (VkFence)fence),
				Error::COMMAND_SUBMISSION_FAILED);
	}

	return {};
}

Res<> VulkanDevice::queue_submit(CommandQueue queue, CommandBuffer cmd,
		SemaphoreSubmitInfo wait_semaphore, SemaphoreSubmitInfo signal_semaphore, Fence fence) {
	if (!queue || !cmd) {
		return Error::INVALID_HANDLE;
	}

	VkCommandBufferSubmitInfo cmd_info = {};
	cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmd_info.commandBuffer = (VkCommandBuffer)cmd;

	VkSemaphoreSubmitInfo wait_info = {};
	if (wait_semaphore.semaphore) {
		wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		wait_info.semaphore = (VkSemaphore)wait_semaphore.semaphore;
		wait_info.value = wait_semaphore.value;
		wait_info.stageMask = static_cast<VkPipelineStageFlags2>(wait_semaphore.stage_mask);
	}

	VkSemaphoreSubmitInfo signal_info = {};
	if (signal_semaphore.semaphore) {
		signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		signal_info.semaphore = (VkSemaphore)signal_semaphore.semaphore;
		signal_info.value = signal_semaphore.value;
		signal_info.stageMask = static_cast<VkPipelineStageFlags2>(signal_semaphore.stage_mask);
	}

	VkSubmitInfo2 submit_info = {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submit_info.commandBufferInfoCount = 1;
	submit_info.pCommandBufferInfos = &cmd_info;
	submit_info.waitSemaphoreInfoCount = wait_semaphore.semaphore ? 1 : 0;
	submit_info.pWaitSemaphoreInfos = wait_semaphore.semaphore ? &wait_info : nullptr;
	submit_info.signalSemaphoreInfoCount = signal_semaphore.semaphore ? 1 : 0;
	submit_info.pSignalSemaphoreInfos = signal_semaphore.semaphore ? &signal_info : nullptr;

	VulkanQueue* vk_queue = (VulkanQueue*)queue;
	if (vk_queue->mutex) {
		std::lock_guard<std::mutex> lock(*vk_queue->mutex);
		VK_CHECK_RET(vkQueueSubmit2(vk_queue->queue, 1, &submit_info, (VkFence)fence),
				Error::COMMAND_SUBMISSION_FAILED);
	} else {
		VK_CHECK_RET(vkQueueSubmit2(vk_queue->queue, 1, &submit_info, (VkFence)fence),
				Error::COMMAND_SUBMISSION_FAILED);
	}

	return {};
}

Res<> VulkanDevice::queue_present(
		CommandQueue queue, Swapchain swapchain, Semaphore wait_semaphore) {
	if (!queue || !swapchain) {
		return Error::INVALID_HANDLE;
	}

	VulkanSwapchain* vk_swapchain = (VulkanSwapchain*)swapchain;
	VulkanQueue* vk_queue = (VulkanQueue*)queue;

	VkSemaphore vk_wait_sem = (VkSemaphore)wait_semaphore;

	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = wait_semaphore == nullptr ? 0 : 1;
	present_info.pWaitSemaphores = wait_semaphore ? &vk_wait_sem : nullptr;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &vk_swapchain->vk_swapchain;
	present_info.pImageIndices = &vk_swapchain->image_index;

	VkResult res;
	if (vk_queue->mutex) {
		// Lock queue for thread safe access
		std::lock_guard<std::mutex> lock(*vk_queue->mutex);
		res = vkQueuePresentKHR(vk_queue->queue, &present_info);
	} else {
		res = vkQueuePresentKHR(vk_queue->queue, &present_info);
	}

	if (res == VK_SUCCESS) {
		return {};
	} else if (res == VK_ERROR_OUT_OF_DATE_KHR) {
		return Error::SWAPCHAIN_OUT_OF_DATE;
	} else if (res == VK_SUBOPTIMAL_KHR) {
		return Error::SWAPCHAIN_SUBOPTIMAL;
	} else {
		GPUKIT_LOG_ERROR("[VULKAN] Queue present failed: {}", vk_result_to_string(res));
		return Error::PRESENTATION_FAILED;
	}
}

} //namespace gpukitkit
