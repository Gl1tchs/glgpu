#include "platform/vulkan/vk_device.h"

#include "platform/vulkan/vk_common.h"

namespace gpukit {

Res<Buffer> VulkanDevice::buffer_create(
		uint64_t size, BufferUsageFlags usage, MemoryAllocationType allocation_type) {
	VkBufferCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	create_info.pNext = nullptr;
	create_info.size = size;
	create_info.usage = usage;

	VmaAllocationCreateInfo alloc_create_info = {};
	switch (allocation_type) {
		case MemoryAllocationType::CPU: {
			bool is_src = usage & BUFFER_USAGE_TRANSFER_SRC_BIT;
			bool is_dst = usage & BUFFER_USAGE_TRANSFER_DST_BIT;

			alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

			alloc_create_info.requiredFlags =
					(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

			if (is_src && !is_dst) {
				// Staging buffer
				alloc_create_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
			} else if (is_dst && !is_src) {
				// Readback buffer
				alloc_create_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
			} else {
				// General purpose - ensure it can be read/written
				alloc_create_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
			}
		} break;
		case MemoryAllocationType::GPU: {
			alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

			if (size <= SMALL_ALLOCATION_MAX_SIZE) {
				uint32_t mem_type_index = 0;
				VkResult res = vmaFindMemoryTypeIndexForBufferInfo(
						_allocator, &create_info, &alloc_create_info, &mem_type_index);

				if (res == VK_SUCCESS) {
					alloc_create_info.pool = _find_or_create_small_allocs_pool(mem_type_index);
				}
			}
		} break;
	}

	// allocate the buffer
	VkBuffer vk_buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = nullptr;
	VmaAllocationInfo alloc_info = {};

	VK_CHECK_RET(vmaCreateBuffer(_allocator, &create_info, &alloc_create_info, &vk_buffer,
						 &allocation, &alloc_info),
			make_err<Buffer>(Error::BUFFER_CREATION_FAILED));

	// Bookkeep.
	VulkanBuffer* buf_info = VersatileResource::allocate<VulkanBuffer>(_resources_allocator);
	buf_info->vk_buffer = vk_buffer;
	buf_info->allocation.handle = allocation;
	buf_info->allocation.size = alloc_info.size;
	buf_info->size = size;
	buf_info->allocation_type = allocation_type;

	return Buffer(buf_info);
}

Res<> VulkanDevice::buffer_free(Buffer buffer) {
	if (!buffer) {
		return {}; // freeing null is a no-op success
	}

	VulkanBuffer* vk_buf = (VulkanBuffer*)buffer;

	// Check for validity before using _device or _allocator
	if (vk_buf->vk_view) {
		vkDestroyBufferView(_device, vk_buf->vk_view, nullptr);
	}

	vmaDestroyBuffer(_allocator, vk_buf->vk_buffer, vk_buf->allocation.handle);

	VersatileResource::free(_resources_allocator, buffer);

	return {};
}

Res<BufferDeviceAddress> VulkanDevice::buffer_get_device_address(Buffer buffer) {
	if (!buffer) {
		return make_err<BufferDeviceAddress>(Error::INVALID_HANDLE);
	}

	VulkanBuffer* vk_buf = (VulkanBuffer*)buffer;

	VkBufferDeviceAddressInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR;
	info.buffer = vk_buf->vk_buffer;

	// vkGetBufferDeviceAddress returns 0 on error/invalid usage,
	// though the spec says it "returns" the address.
	VkDeviceAddress addr = vkGetBufferDeviceAddress(_device, &info);
	if (addr == 0) {
		return make_err<BufferDeviceAddress>(
				Error::INVALID_OPERATION); // Likely buffer wasn't created with
										   // SHADER_DEVICE_ADDRESS_BIT
	}

	return addr;
}

Res<uint8_t*> VulkanDevice::buffer_map(Buffer buffer) {
	if (!buffer)
		return make_err<uint8_t*>(Error::INVALID_HANDLE);

	VulkanBuffer* vk_buf = (VulkanBuffer*)buffer;

	void* data_ptr = nullptr;
	// Map failures usually indicate out of host memory or memory not being mappable
	VK_CHECK_RET(vmaMapMemory(_allocator, vk_buf->allocation.handle, &data_ptr),
			make_err<uint8_t*>(Error::OUT_OF_HOST_MEMORY));

	return (uint8_t*)data_ptr;
}

Res<> VulkanDevice::buffer_unmap(Buffer buffer) {
	if (!buffer)
		return Error::INVALID_HANDLE;

	VulkanBuffer* vk_buf = (VulkanBuffer*)buffer;

	vmaUnmapMemory(_allocator, vk_buf->allocation.handle);
	return {};
}

Res<> VulkanDevice::buffer_invalidate(Buffer buffer) {
	if (!buffer)
		return Error::INVALID_HANDLE;

	VulkanBuffer* vk_buf = (VulkanBuffer*)buffer;

	VK_CHECK_RET(vmaInvalidateAllocation(_allocator, vk_buf->allocation.handle, 0, VK_WHOLE_SIZE),
			Error::INVALID_OPERATION);

	return {};
}

Res<> VulkanDevice::buffer_flush(Buffer buffer) {
	if (!buffer)
		return Error::INVALID_HANDLE;

	VulkanBuffer* vk_buf = (VulkanBuffer*)buffer;

	VK_CHECK_RET(vmaFlushAllocation(_allocator, vk_buf->allocation.handle, 0, VK_WHOLE_SIZE),
			Error::INVALID_OPERATION);

	return {};
}

VmaPool VulkanDevice::_find_or_create_small_allocs_pool(uint32_t mem_type_index) {
	if (_small_allocs_pools.find(mem_type_index) != _small_allocs_pools.end()) {
		return _small_allocs_pools[mem_type_index];
	}

	VmaPoolCreateInfo pci = {};
	pci.memoryTypeIndex = mem_type_index;
	pci.flags = 0;
	pci.blockSize = 0;
	pci.minBlockCount = 0;
	pci.maxBlockCount = SIZE_MAX;
	pci.priority = 0.5f;
	pci.minAllocationAlignment = 0;
	pci.pMemoryAllocateNext = nullptr;

	VmaPool pool = VK_NULL_HANDLE;
	VkResult res = vmaCreatePool(_allocator, &pci, &pool);

	if (res != VK_SUCCESS) {
		GPUKIT_LOG_ERROR("[VMA] Failed to create small alloc pool: {}", vk_result_to_string(res));
		return VK_NULL_HANDLE;
	}

	_small_allocs_pools[mem_type_index] = pool;

	return pool;
}

Res<> VulkanDevice::buffer_upload(
		Buffer buffer, const void* data, size_t size, size_t offset) {
	if (!buffer || !data || size == 0) {
		return Error::INVALID_ARGUMENT;
	}

	VulkanBuffer* vk_buf = (VulkanBuffer*)buffer;

	if (vk_buf->allocation_type == MemoryAllocationType::CPU) {
		auto map_res = buffer_map(buffer);
		if (map_res.is_error()) {
			return map_res.error();
		}
		memcpy(map_res.value() + offset, data, size);
		buffer_unmap(buffer);
		return {};
	}

	auto staging_res =
			buffer_create(size, BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryAllocationType::CPU);
	if (staging_res.is_error()) {
		return staging_res.error();
	}

	Buffer staging = staging_res.value();

	auto smap_res = buffer_map(staging);
	if (smap_res.is_error()) {
		buffer_free(staging);
		return smap_res.error();
	}
	memcpy(smap_res.value(), data, size);
	buffer_unmap(staging);

	Res<> submit_res = command_immediate_submit([&](CommandBuffer cmd) {
		BufferCopyRegion region = { 0, offset, size };
		command_copy_buffer(cmd, staging, buffer, { region });
	});

	buffer_free(staging);
	return submit_res;
}

} // namespace gl
