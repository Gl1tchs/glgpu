#include "platform/vulkan/vk_device.h"

#include <vulkan/vulkan_core.h>

namespace gpukit {

Res<QueryPool> VulkanDevice::query_pool_create(uint32_t count) {
	if (count == 0) {
		return make_err<QueryPool>(Error::INVALID_ARGUMENT);
	}

	VkQueryPoolCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	create_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
	create_info.queryCount = count;

	VkQueryPool vk_pool = VK_NULL_HANDLE;
	VK_CHECK_RET(vkCreateQueryPool(_device, &create_info, nullptr, &vk_pool),
			make_err<QueryPool>(Error::INITIALIZATION_FAILED));

	VulkanQueryPool* pool = VersatileResource::allocate<VulkanQueryPool>(_resources_allocator);
	if (!pool) {
		vkDestroyQueryPool(_device, vk_pool, nullptr);
		return make_err<QueryPool>(Error::OUT_OF_HOST_MEMORY);
	}

	pool->vk_query_pool = vk_pool;
	pool->count = count;

	return QueryPool(pool);
}

Res<> VulkanDevice::query_pool_free(QueryPool pool) {
	if (!pool) {
		return {};
	}

	VulkanQueryPool* vk_pool = (VulkanQueryPool*)pool;
	vkDestroyQueryPool(_device, vk_pool->vk_query_pool, nullptr);
	VersatileResource::free(_resources_allocator, vk_pool);

	return {};
}

Res<std::vector<uint64_t>> VulkanDevice::query_pool_get_results(
		QueryPool pool, uint32_t first, uint32_t count) {
	if (!pool) {
		return make_err<std::vector<uint64_t>>(Error::INVALID_HANDLE);
	}

	VulkanQueryPool* vk_pool = (VulkanQueryPool*)pool;

	uint32_t query_count = (count == UINT32_MAX) ? vk_pool->count - first : count;

	std::vector<uint64_t> results(query_count);

	VkResult res = vkGetQueryPoolResults(_device, vk_pool->vk_query_pool, first, query_count,
			query_count * sizeof(uint64_t), results.data(), sizeof(uint64_t),
			VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

	if (res != VK_SUCCESS && res != VK_NOT_READY) {
		return make_err<std::vector<uint64_t>>(Error::INVALID_OPERATION);
	}

	return results;
}

double VulkanDevice::timestamps_to_ns(uint64_t ticks) const {
	return static_cast<double>(ticks) * _physical_device_properties.limits.timestampPeriod;
}

} // namespace gpukit
