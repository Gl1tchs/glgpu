#include "platform/vulkan/vk_device.h"

#include "platform/vulkan/vk_common.h"

namespace gpukit {

static VkDescriptorType _resolve_descriptor_type(
		VulkanDevice::VulkanUniformSet* usi, uint32_t binding, VkDescriptorType default_type) {
	if (usi && usi->shader) {
		auto set_it = usi->shader->reflected_bindings.find(usi->set_index);
		if (set_it != usi->shader->reflected_bindings.end()) {
			auto binding_it = set_it->second.find(binding);
			if (binding_it != set_it->second.end()) {
				return binding_it->second.type;
			}
		}
	}
	return default_type;
}

Res<UniformSet> VulkanDevice::uniform_set_create(
		VectorView<ShaderUniform> uniforms, Shader shader, uint32_t set_index) {
	const VulkanShader* shader_info = (const VulkanShader*)shader;
	if (!shader_info) {
		return make_err<UniformSet>(Error::INVALID_HANDLE);
	}

	if (set_index >= shader_info->descriptor_set_layouts.size()) {
		return make_err<UniformSet>(Error::UNIFORM_SET_INVALID_SET_INDEX);
	}

	DescriptorSetPoolKey pool_key;
	std::vector<VkWriteDescriptorSet> vk_writes;

	// We need stable memory addresses for pImageInfo/pBufferInfo pointers in VkWriteDescriptorSet
	// Using maps ensures pointer stability as elements are added.
	std::map<size_t, std::vector<VkDescriptorImageInfo>> vk_image_infos;
	std::map<size_t, VkDescriptorBufferInfo> vk_buffer_infos;

	for (uint32_t i = 0; i < uniforms.size(); i++) {
		const ShaderUniform& uniform = uniforms[i];

		VkWriteDescriptorSet vk_write = {};
		vk_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		vk_write.dstBinding = uniform.binding;
		vk_write.descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM; // Placeholder

		uint32_t num_descriptors = 1;

		switch (uniform.type) {
			case ShaderUniformType::SAMPLER: {
				num_descriptors = static_cast<uint32_t>(uniform.data.size());
				vk_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;

				for (uint32_t j = 0; j < num_descriptors; j++) {
					VkDescriptorImageInfo vk_img_info = {};
					vk_img_info.sampler = (VkSampler)uniform.data[j];
					vk_img_info.imageView = VK_NULL_HANDLE;
					vk_img_info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
					vk_image_infos[i].push_back(vk_img_info);
				}
			} break;
			case ShaderUniformType::SAMPLER_WITH_TEXTURE: {
				// Data layout: [Sampler, Image, Sampler, Image...]
				if (uniform.data.size() % 2 != 0)
					return make_err<UniformSet>(Error::INVALID_ARGUMENT);
				num_descriptors = static_cast<uint32_t>(uniform.data.size()) / 2;
				vk_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

				for (uint32_t j = 0; j < num_descriptors; j++) {
					VkDescriptorImageInfo vk_img_info = {};
					vk_img_info.sampler = (VkSampler)uniform.data[j * 2 + 0];
					VulkanImage* img = (VulkanImage*)uniform.data[j * 2 + 1];
					if (!img) {
						return make_err<UniformSet>(Error::INVALID_HANDLE);
					}

					vk_img_info.imageView = img->vk_image_view;
					vk_img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					vk_image_infos[i].push_back(vk_img_info);
				}
			} break;
			case ShaderUniformType::TEXTURE: {
				num_descriptors = static_cast<uint32_t>(uniform.data.size());
				vk_write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

				for (uint32_t j = 0; j < num_descriptors; j++) {
					VkDescriptorImageInfo vk_img_info = {};
					VulkanImage* img = (VulkanImage*)uniform.data[j];
					if (!img) {
						return make_err<UniformSet>(Error::INVALID_HANDLE);
					}

					vk_img_info.imageView = img->vk_image_view;
					vk_img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					vk_image_infos[i].push_back(vk_img_info);
				}
			} break;
			case ShaderUniformType::IMAGE: {
				num_descriptors = static_cast<uint32_t>(uniform.data.size());
				vk_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

				for (uint32_t j = 0; j < num_descriptors; j++) {
					VkDescriptorImageInfo vk_img_info = {};
					VulkanImage* img = (VulkanImage*)uniform.data[j];
					if (!img) {
						return make_err<UniformSet>(Error::INVALID_HANDLE);
					}

					vk_img_info.imageView = img->vk_image_view;
					vk_img_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
					vk_image_infos[i].push_back(vk_img_info);
				}
			} break;
			case ShaderUniformType::UNIFORM_BUFFER: {
				if (uniform.data.empty())
					return make_err<UniformSet>(Error::INVALID_ARGUMENT);
				const VulkanBuffer* buf_info = (const VulkanBuffer*)uniform.data[0];
				if (!buf_info) {
					return make_err<UniformSet>(Error::INVALID_HANDLE);
				}

				vk_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

				VkDescriptorBufferInfo vk_buf_info = {};
				vk_buf_info.buffer = buf_info->vk_buffer;
				vk_buf_info.range = buf_info->size;
				vk_buffer_infos[i] = vk_buf_info;
			} break;
			case ShaderUniformType::STORAGE_BUFFER: {
				if (uniform.data.empty()) {
					return make_err<UniformSet>(Error::INVALID_ARGUMENT);
				}

				const VulkanBuffer* buf_info = (const VulkanBuffer*)uniform.data[0];
				if (!buf_info) {
					return make_err<UniformSet>(Error::INVALID_HANDLE);
				}

				vk_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

				VkDescriptorBufferInfo vk_buf_info = {};
				vk_buf_info.buffer = buf_info->vk_buffer;
				vk_buf_info.range = buf_info->size;
				vk_buffer_infos[i] = vk_buf_info;
			} break;
			default: {
				return make_err<UniformSet>(Error::UNIMPLEMENTED);
			}
		}

		vk_write.descriptorCount = num_descriptors;
		vk_writes.push_back(vk_write);

		// Update pool key
		const uint32_t type_int = static_cast<uint32_t>(uniform.type);
		if (pool_key.uniform_type[type_int] + num_descriptors > MAX_UNIFORM_POOL_ELEMENT) {
			GPUKIT_LOG_ERROR("[VULKAN] Uniform set descriptor limit exceeded.");
			return make_err<UniformSet>(Error::DESCRIPTOR_POOL_EXHAUSTED);
		}

		pool_key.uniform_type[type_int] += static_cast<uint16_t>(num_descriptors);
	}

	// Need a descriptor pool.
	VkDescriptorPool vk_pool = _uniform_pool_find_or_create(pool_key);
	if (vk_pool == VK_NULL_HANDLE) {
		return make_err<UniformSet>(Error::DESCRIPTOR_POOL_EXHAUSTED);
	}

	VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {};
	descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptor_set_allocate_info.descriptorPool = vk_pool;
	descriptor_set_allocate_info.descriptorSetCount = 1;
	descriptor_set_allocate_info.pSetLayouts = &shader_info->descriptor_set_layouts[set_index];

	VkDescriptorSet vk_descriptor_set = VK_NULL_HANDLE;
	VkResult res =
			vkAllocateDescriptorSets(_device, &descriptor_set_allocate_info, &vk_descriptor_set);

	if (res != VK_SUCCESS) {
		_uniform_pool_unreference(pool_key, vk_pool);
		GPUKIT_LOG_ERROR(
				"[VULKAN] Failed to allocate descriptor sets: {}", vk_result_to_string(res));
		return make_err<UniformSet>(Error::DESCRIPTOR_POOL_EXHAUSTED);
	}

	// Link write infos to the actual descriptor set
	for (const auto& img_info : vk_image_infos) {
		// img_info.first is the index in vk_writes corresponding to 'i' in main loop
		// We need to find the correct write struct.
		if (img_info.first < vk_writes.size()) {
			vk_writes[img_info.first].pImageInfo = img_info.second.data();
		}
	}

	for (const auto& buf_info : vk_buffer_infos) {
		if (buf_info.first < vk_writes.size()) {
			vk_writes[buf_info.first].pBufferInfo = &buf_info.second;
		}
	}

	for (auto& write : vk_writes) {
		write.dstSet = vk_descriptor_set;
	}

	vkUpdateDescriptorSets(
			_device, static_cast<uint32_t>(vk_writes.size()), vk_writes.data(), 0, nullptr);

	// Bookkeep.
	VulkanUniformSet* usi = VersatileResource::allocate<VulkanUniformSet>(_resources_allocator);
	if (!usi) {
		return make_err<UniformSet>(Error::OUT_OF_HOST_MEMORY);
	}

	usi->vk_descriptor_set = vk_descriptor_set;
	usi->vk_descriptor_pool = vk_pool;
	usi->pool_key = pool_key;
	usi->shader = const_cast<VulkanShader*>(shader_info);
	usi->set_index = set_index;

	return UniformSet(usi);
}

Res<UniformSet> VulkanDevice::uniform_set_create(Shader shader, uint32_t set_index) {
	const VulkanShader* shader_info = (const VulkanShader*)shader;
	if (!shader_info) {
		return make_err<UniformSet>(Error::INVALID_HANDLE);
	}

	if (set_index >= shader_info->descriptor_set_layouts.size()) {
		return make_err<UniformSet>(Error::UNIFORM_SET_INVALID_SET_INDEX);
	}

	DescriptorSetPoolKey pool_key = {};
	auto set_it = shader_info->reflected_bindings.find(set_index);
	if (set_it != shader_info->reflected_bindings.end()) {
		for (auto& [binding_index, binding] : set_it->second) {
			ShaderUniformType uniform_type;
			switch (binding.type) {
				case VK_DESCRIPTOR_TYPE_SAMPLER:
					uniform_type = ShaderUniformType::SAMPLER;
					break;
				case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
					uniform_type = ShaderUniformType::SAMPLER_WITH_TEXTURE;
					break;
				case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
					uniform_type = ShaderUniformType::TEXTURE;
					break;
				case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
					uniform_type = ShaderUniformType::IMAGE;
					break;
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
					uniform_type = ShaderUniformType::UNIFORM_BUFFER;
					break;
				case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
					uniform_type = ShaderUniformType::STORAGE_BUFFER;
					break;
				default:
					continue;
			}
			uint32_t type_int = static_cast<uint32_t>(uniform_type);
			pool_key.uniform_type[type_int] += static_cast<uint16_t>(binding.count);
		}
	}

	VkDescriptorPool vk_pool = _uniform_pool_find_or_create(pool_key);
	if (vk_pool == VK_NULL_HANDLE) {
		return make_err<UniformSet>(Error::DESCRIPTOR_POOL_EXHAUSTED);
	}

	VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {};
	descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptor_set_allocate_info.descriptorPool = vk_pool;
	descriptor_set_allocate_info.descriptorSetCount = 1;
	descriptor_set_allocate_info.pSetLayouts = &shader_info->descriptor_set_layouts[set_index];

	VkDescriptorSet vk_descriptor_set = VK_NULL_HANDLE;
	VkResult res =
			vkAllocateDescriptorSets(_device, &descriptor_set_allocate_info, &vk_descriptor_set);

	if (res != VK_SUCCESS) {
		_uniform_pool_unreference(pool_key, vk_pool);
		GPUKIT_LOG_ERROR(
				"[VULKAN] Failed to allocate descriptor sets: {}", vk_result_to_string(res));
		return make_err<UniformSet>(Error::DESCRIPTOR_POOL_EXHAUSTED);
	}

	VulkanUniformSet* usi = VersatileResource::allocate<VulkanUniformSet>(_resources_allocator);
	if (!usi) {
		vkFreeDescriptorSets(_device, vk_pool, 1, &vk_descriptor_set);
		_uniform_pool_unreference(pool_key, vk_pool);
		return make_err<UniformSet>(Error::OUT_OF_HOST_MEMORY);
	}

	usi->vk_descriptor_set = vk_descriptor_set;
	usi->vk_descriptor_pool = vk_pool;
	usi->pool_key = pool_key;
	usi->shader = const_cast<VulkanShader*>(shader_info);
	usi->set_index = set_index;

	return UniformSet(usi);
}

Res<> VulkanDevice::uniform_set_free(UniformSet uniform_set) {
	VulkanUniformSet* usi = (VulkanUniformSet*)uniform_set;
	if (!usi) {
		return {};
	}

	if (usi->vk_descriptor_pool && usi->vk_descriptor_set) {
		vkFreeDescriptorSets(_device, usi->vk_descriptor_pool, 1, &usi->vk_descriptor_set);
		_uniform_pool_unreference(usi->pool_key, usi->vk_descriptor_pool);
	}

	VersatileResource::free(_resources_allocator, usi);

	return {};
}

Res<UniformSet> VulkanDevice::uniform_set_create_bindless(
		Shader shader, uint32_t set_index, uint32_t binding_index, uint32_t max_count) {
	VulkanShader* shader_info = (VulkanShader*)shader;
	if (!shader_info) {
		return make_err<UniformSet>(Error::INVALID_HANDLE);
	}

	if (set_index >= shader_info->descriptor_set_layouts.size()) {
		return make_err<UniformSet>(Error::UNIFORM_SET_INVALID_SET_INDEX);
	}

	// NOTE: We need a dedicated pool with the UPDATE_AFTER_BIND flag.
	// This allows us to update the descriptor set while it is bound to a command buffer in flight.
	// We populate the pool with support for all bindless types we want to use.
	std::vector<VkDescriptorPoolSize> pool_sizes = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, max_count },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_count },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, max_count },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, max_count },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, max_count },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, max_count },
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT |
			VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	pool_info.maxSets = 1;
	pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
	pool_info.pPoolSizes = pool_sizes.data();

	VkDescriptorPool vk_pool = VK_NULL_HANDLE;
	if (vkCreateDescriptorPool(_device, &pool_info, nullptr, &vk_pool) != VK_SUCCESS) {
		return make_err<UniformSet>(Error::DESCRIPTOR_POOL_EXHAUSTED);
	}

	// Allocate the descriptor set
	VkDescriptorSetAllocateInfo alloc_info = {};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = vk_pool;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &shader_info->descriptor_set_layouts[set_index];

	VkDescriptorSet vk_set = VK_NULL_HANDLE;
	if (vkAllocateDescriptorSets(_device, &alloc_info, &vk_set) != VK_SUCCESS) {
		vkDestroyDescriptorPool(_device, vk_pool, nullptr);
		return make_err<UniformSet>(Error::DESCRIPTOR_POOL_EXHAUSTED);
	}

	// Bookkeep
	VulkanUniformSet* usi = VersatileResource::allocate<VulkanUniformSet>(_resources_allocator);
	if (!usi) {
		return make_err<UniformSet>(Error::OUT_OF_HOST_MEMORY);
	}

	usi->vk_descriptor_set = vk_set;
	usi->vk_descriptor_pool = vk_pool;
	usi->bindless = true;
	usi->shader = shader_info;
	usi->set_index = set_index;

	// Store a dummy key so the freeing logic doesn't crash,
	// though we might want to manually manage this pool's lifecycle
	DescriptorSetPoolKey key = {};
	usi->pool_key = key;

	// Hack: Register this pool in the map so uniform_set_free cleans it up correctly
	_descriptor_set_pools[key][vk_pool] = 1;

	return UniformSet(usi);
}

Res<> VulkanDevice::uniform_set_update_texture(
		UniformSet set, uint32_t binding, uint32_t array_index, Image image, Sampler sampler) {
	VulkanUniformSet* usi = (VulkanUniformSet*)set;
	VulkanImage* vk_image = (VulkanImage*)image;
	VkSampler vk_sampler = (VkSampler)sampler;
	if (!usi || !vk_image || !vk_sampler) {
		return Error::INVALID_HANDLE;
	}

	VkDescriptorType descriptor_type =
			_resolve_descriptor_type(usi, binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	VkDescriptorImageInfo image_info = {};
	image_info.imageView = vk_image->vk_image_view;
	image_info.sampler = vk_sampler;
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = usi->vk_descriptor_set;
	write.dstBinding = binding;
	write.dstArrayElement = array_index;
	write.descriptorType = descriptor_type;
	write.descriptorCount = 1;
	write.pImageInfo = &image_info;

	vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);

	return {};
}

Res<> VulkanDevice::uniform_set_update_sampled_image(
		UniformSet set, uint32_t binding, uint32_t array_index, Image image) {
	VulkanUniformSet* usi = (VulkanUniformSet*)set;
	VulkanImage* vk_image = (VulkanImage*)image;
	if (!usi || !vk_image) {
		return Error::INVALID_HANDLE;
	}

	VkDescriptorType descriptor_type =
			_resolve_descriptor_type(usi, binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	VkDescriptorImageInfo image_info = {};
	image_info.imageView = vk_image->vk_image_view;
	image_info.sampler = VK_NULL_HANDLE;
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = usi->vk_descriptor_set;
	write.dstBinding = binding;
	write.dstArrayElement = array_index;
	write.descriptorType = descriptor_type;
	write.descriptorCount = 1;
	write.pImageInfo = &image_info;

	vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);

	return {};
}

Res<> VulkanDevice::uniform_set_update_storage_image(
		UniformSet set, uint32_t binding, uint32_t array_index, Image image) {
	VulkanUniformSet* usi = (VulkanUniformSet*)set;
	VulkanImage* vk_image = (VulkanImage*)image;
	if (!usi || !vk_image) {
		return Error::INVALID_HANDLE;
	}

	VkDescriptorType descriptor_type =
			_resolve_descriptor_type(usi, binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	VkDescriptorImageInfo image_info = {};
	image_info.imageView = vk_image->vk_image_view;
	image_info.sampler = VK_NULL_HANDLE;
	image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = usi->vk_descriptor_set;
	write.dstBinding = binding;
	write.dstArrayElement = array_index;
	write.descriptorType = descriptor_type;
	write.descriptorCount = 1;
	write.pImageInfo = &image_info;

	vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);

	return {};
}

Res<> VulkanDevice::uniform_set_update_buffer(
		UniformSet set, uint32_t binding, uint32_t array_index, Buffer buffer) {
	VulkanUniformSet* usi = (VulkanUniformSet*)set;
	VulkanBuffer* vk_buffer = (VulkanBuffer*)buffer;
	if (!usi || !vk_buffer) {
		return Error::INVALID_HANDLE;
	}

	VkDescriptorType descriptor_type =
			_resolve_descriptor_type(usi, binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	VkDescriptorBufferInfo buffer_info = {};
	buffer_info.buffer = vk_buffer->vk_buffer;
	buffer_info.offset = 0;
	buffer_info.range = vk_buffer->size;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = usi->vk_descriptor_set;
	write.dstBinding = binding;
	write.dstArrayElement = array_index;
	write.descriptorType = descriptor_type;
	write.descriptorCount = 1;
	write.pBufferInfo = &buffer_info;

	vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);

	return {};
}

static const uint32_t MAX_DESCRIPTOR_SETS_PER_POOL = 10;

VkDescriptorPool VulkanDevice::_uniform_pool_find_or_create(const DescriptorSetPoolKey& key) {
	// Try to find existing pool with space
	auto it = _descriptor_set_pools.find(key);
	if (it != _descriptor_set_pools.end()) {
		for (auto& pair : it->second) {
			if (pair.second < MAX_DESCRIPTOR_SETS_PER_POOL) {
				pair.second++; // Increment ref count
				return pair.first;
			}
		}
	}

	// Create a new one.
	std::vector<VkDescriptorPoolSize> vk_sizes;

	auto add_pool_size = [&](ShaderUniformType type, VkDescriptorType vk_type) {
		uint16_t count = key.uniform_type[static_cast<uint32_t>(type)];
		if (count > 0) {
			VkDescriptorPoolSize size = {};
			size.type = vk_type;
			size.descriptorCount = count * MAX_DESCRIPTOR_SETS_PER_POOL;
			vk_sizes.push_back(size);
		}
	};

	add_pool_size(ShaderUniformType::SAMPLER, VK_DESCRIPTOR_TYPE_SAMPLER);
	add_pool_size(
			ShaderUniformType::SAMPLER_WITH_TEXTURE, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	add_pool_size(ShaderUniformType::TEXTURE, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	add_pool_size(ShaderUniformType::IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	add_pool_size(ShaderUniformType::UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	add_pool_size(ShaderUniformType::STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// If no descriptors are needed, we can't create a valid pool with 0 size
	if (vk_sizes.empty())
		return VK_NULL_HANDLE;

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT |
			VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	pool_info.maxSets = MAX_DESCRIPTOR_SETS_PER_POOL;
	pool_info.poolSizeCount = static_cast<uint32_t>(vk_sizes.size());
	pool_info.pPoolSizes = vk_sizes.data();

	VkDescriptorPool vk_pool = VK_NULL_HANDLE;
	if (vkCreateDescriptorPool(_device, &pool_info, nullptr, &vk_pool) != VK_SUCCESS) {
		return VK_NULL_HANDLE;
	}

	// Bookkeep.
	_descriptor_set_pools[key][vk_pool] = 1;

	return vk_pool;
}

void VulkanDevice::_uniform_pool_unreference(
		const DescriptorSetPoolKey& key, VkDescriptorPool vk_descriptor_pool) {
	auto pool_sets_it = _descriptor_set_pools.find(key);
	if (pool_sets_it == _descriptor_set_pools.end()) {
		return;
	}

	auto pool_rcs_it = pool_sets_it->second.find(vk_descriptor_pool);
	if (pool_rcs_it == pool_sets_it->second.end()) {
		return;
	}

	pool_rcs_it->second--;
	if (pool_rcs_it->second == 0) {
		vkDestroyDescriptorPool(_device, vk_descriptor_pool, nullptr);
		pool_sets_it->second.erase(vk_descriptor_pool);
		if (pool_sets_it->second.empty()) {
			_descriptor_set_pools.erase(pool_sets_it);
		}
	}
}

Res<> VulkanDevice::uniform_set_update_buffer_range(UniformSet set, uint32_t binding,
		uint32_t array_index, Buffer buffer, uint64_t offset, uint64_t range) {
	VulkanUniformSet* usi = (VulkanUniformSet*)set;
	VulkanBuffer* vk_buffer = (VulkanBuffer*)buffer;
	if (!usi || !vk_buffer) {
		return Error::INVALID_HANDLE;
	}

	VkDescriptorType descriptor_type =
			_resolve_descriptor_type(usi, binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	VkDescriptorBufferInfo buffer_info = {};
	buffer_info.buffer = vk_buffer->vk_buffer;
	buffer_info.offset = offset;
	buffer_info.range = range;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = usi->vk_descriptor_set;
	write.dstBinding = binding;
	write.dstArrayElement = array_index;
	write.descriptorType = descriptor_type;
	write.descriptorCount = 1;
	write.pBufferInfo = &buffer_info;

	vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);

	return {};
}

} //namespace gpukit
