#include "platform/vulkan/vk_device.h"

namespace gpukit {

static VkImageUsageFlags _gl_to_vk_image_usage_flags(ImageUsageFlags usage) {
	VkImageUsageFlags vk_usage = 0;
	if (usage & IMAGE_USAGE_TRANSFER_SRC_BIT) {
		vk_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}
	if (usage & IMAGE_USAGE_TRANSFER_DST_BIT) {
		vk_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	if (usage & IMAGE_USAGE_SAMPLED_BIT) {
		vk_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
	}
	if (usage & IMAGE_USAGE_STORAGE_BIT) {
		vk_usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	}
	if (usage & IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
		vk_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}
	if (usage & IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
		vk_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}

	return vk_usage;
}

VulkanDevice::VulkanImage* VulkanDevice::_image_create(VkFormat format, VkExtent3D size,
		VkImageUsageFlags usage, bool mipmapped, VkSampleCountFlagBits samples,
		uint32_t array_layers, bool cube_map) {
	const uint32_t mip_levels = mipmapped
			? static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1
			: 1;

	VkImageCreateInfo img_info = {};
	img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	img_info.pNext = nullptr;
	img_info.imageType = VK_IMAGE_TYPE_2D;
	img_info.format = format;
	img_info.extent = size;
	img_info.mipLevels = mip_levels;
	img_info.arrayLayers = array_layers;
	img_info.samples = samples;
	img_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	img_info.usage = usage;
	if (cube_map) {
		img_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	}

	VmaAllocationCreateInfo alloc_info = {};
	alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	alloc_info.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	VkImage vk_image = VK_NULL_HANDLE;
	VmaAllocation vma_allocation = {};

	if (vmaCreateImage(_allocator, &img_info, &alloc_info, &vk_image, &vma_allocation, nullptr) !=
			VK_SUCCESS) {
		GPUKIT_LOG_ERROR("[VULKAN] Failed to create image via VMA");
		return nullptr;
	}

	VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
	if (img_info.format == VK_FORMAT_D32_SFLOAT) {
		aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
	if (cube_map) {
		view_type = VK_IMAGE_VIEW_TYPE_CUBE;
	} else if (array_layers > 1) {
		view_type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	}

	VkImageViewCreateInfo view_info = {};
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.pNext = nullptr;
	view_info.viewType = view_type;
	view_info.image = vk_image;
	view_info.format = format;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = mip_levels;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = array_layers;
	view_info.subresourceRange.aspectMask = aspect_flags;

	VkImageView vk_image_view = VK_NULL_HANDLE;
	if (vkCreateImageView(_device, &view_info, nullptr, &vk_image_view) != VK_SUCCESS) {
		GPUKIT_LOG_ERROR("[VULKAN] Failed to create image view");
		vmaDestroyImage(_allocator, vk_image, vma_allocation);
		return nullptr;
	}

	VulkanImage* image = VersatileResource::allocate<VulkanImage>(_resources_allocator);
	if (!image) {
		vkDestroyImageView(_device, vk_image_view, nullptr);
		vmaDestroyImage(_allocator, vk_image, vma_allocation);
		return nullptr;
	}

	image->vk_image = vk_image;
	image->vk_image_view = vk_image_view;
	image->allocation = vma_allocation;
	image->image_extent = size;
	image->image_format = format;
	image->mip_levels = mip_levels;
	image->image_usage = usage;
	image->array_layers = array_layers;

	return image;
}

void VulkanDevice::_generate_image_mipmaps(CommandBuffer cmd, Image image, Vec2u size) {
	const uint32_t mip_levels = *image_get_mip_levels(image); // should not fail

	uint32_t mip_width = (uint32_t)size.x;
	uint32_t mip_height = (uint32_t)size.y;

	for (uint32_t i = 1; i < mip_levels; i++) {
		command_transition_image(cmd, image, ImageLayout::TRANSFER_DST_OPTIMAL,
				ImageLayout::TRANSFER_SRC_OPTIMAL, i - 1, 1);

		command_copy_image_to_image(cmd, image, image, { mip_width, mip_height },
				{ mip_width > 1 ? mip_width / 2 : 1, mip_height > 1 ? mip_height / 2 : 1 }, i - 1,
				i);

		command_transition_image(cmd, image, ImageLayout::TRANSFER_SRC_OPTIMAL,
				ImageLayout::SHADER_READ_ONLY_OPTIMAL, i - 1, 1);

		if (mip_width > 1) {
			mip_width /= 2;
		}
		if (mip_height > 1) {
			mip_height /= 2;
		}
	}

	command_transition_image(cmd, image, ImageLayout::TRANSFER_DST_OPTIMAL,
			ImageLayout::SHADER_READ_ONLY_OPTIMAL, mip_levels - 1, 1);
}

Res<Image> VulkanDevice::image_create(const ImageCreateInfo& info) {
	if (info.cube_map && info.array_layers != 6) {
		GPUKIT_LOG_ERROR("[VULKAN] Cubemap requires exactly 6 array layers");
		return make_err<Image>(Error::INVALID_ARGUMENT);
	}
	if (info.cube_map && info.mipmapped) {
		GPUKIT_LOG_ERROR("[VULKAN] Mipmapped cubemaps are not supported");
		return make_err<Image>(Error::INVALID_ARGUMENT);
	}

	VkExtent3D vk_size = { info.size.x, info.size.y, 1 };
	VkFormat vk_format = static_cast<VkFormat>(info.format);
	VkImageUsageFlags vk_usage = _gl_to_vk_image_usage_flags(info.usage);

	if (!info.data) {
		VulkanImage* raw_img = _image_create(vk_format, vk_size, vk_usage, info.mipmapped,
				static_cast<VkSampleCountFlagBits>(info.samples), info.array_layers, info.cube_map);

		if (!raw_img)
			return make_err<Image>(Error::IMAGE_CREATION_FAILED);

		return Image(raw_img);
	} else {
		const size_t layer_size =
				vk_size.width * vk_size.height * get_data_format_size(info.format);
		const size_t data_size = layer_size * info.array_layers;

		Res<Buffer> staging_res =
				buffer_create(data_size, BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryAllocationType::CPU);
		if (staging_res.is_error()) {
			return make_err<Image>(staging_res.error());
		}
		Buffer staging_buffer = staging_res.value();

		Res<uint8_t*> map_res = buffer_map(staging_buffer);
		if (map_res.is_error()) {
			buffer_free(staging_buffer);
			return make_err<Image>(map_res.error());
		}

		memcpy(map_res.value(), info.data, data_size);
		buffer_unmap(staging_buffer);

		VkImageUsageFlags image_usage = vk_usage;
		image_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		image_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		VulkanImage* new_image_raw = _image_create(vk_format, vk_size, image_usage, info.mipmapped,
				static_cast<VkSampleCountFlagBits>(info.samples), info.array_layers, info.cube_map);

		if (!new_image_raw) {
			buffer_free(staging_buffer);
			return make_err<Image>(Error::IMAGE_CREATION_FAILED);
		}

		Image new_image = (Image)new_image_raw;

		std::vector<BufferImageCopyRegion> copy_regions;
		copy_regions.reserve(info.array_layers);
		for (uint32_t layer = 0; layer < info.array_layers; layer++) {
			BufferImageCopyRegion region = {};
			region.buffer_offset = layer * layer_size;
			region.image_subresource.aspect_mask = IMAGE_ASPECT_COLOR_BIT;
			region.image_subresource.mip_level = 0;
			region.image_subresource.base_array_layer = layer;
			region.image_subresource.layer_count = 1;
			region.image_extent = { info.size.x, info.size.y, 1 };
			copy_regions.push_back(region);
		}

		Res<> submit_res = command_immediate_submit(
				[&](CommandBuffer cmd) {
					command_transition_image(cmd, new_image, ImageLayout::UNDEFINED,
							ImageLayout::TRANSFER_DST_OPTIMAL);

					command_copy_buffer_to_image(cmd, staging_buffer, new_image, copy_regions);

					if (info.mipmapped) {
						_generate_image_mipmaps(cmd, new_image, info.size);
					} else {
						command_transition_image(cmd, new_image, ImageLayout::TRANSFER_DST_OPTIMAL,
								ImageLayout::SHADER_READ_ONLY_OPTIMAL);
					}
				},
				QueueType::GRAPHICS);

		buffer_free(staging_buffer);

		if (submit_res.is_error()) {
			image_free(new_image);
			return make_err<Image>(submit_res.error());
		}

		return Image(new_image);
	}
}

Res<> VulkanDevice::image_free(Image image) {
	if (!image) {
		return {};
	}

	VulkanImage* vk_image = (VulkanImage*)image;

	vkDestroyImageView(_device, vk_image->vk_image_view, nullptr);
	vmaDestroyImage(_allocator, vk_image->vk_image, vk_image->allocation);

	VersatileResource::free(_resources_allocator, image);

	return {};
}

Res<Vec3u> VulkanDevice::image_get_size(Image image) {
	if (!image) {
		return make_err<Vec3u>(Error::INVALID_HANDLE);
	}

	VulkanImage* vk_image = (VulkanImage*)image;

	Vec3u size;
	static_assert(sizeof(Vec3u) == sizeof(VkExtent3D));
	memcpy(&size, &vk_image->image_extent, sizeof(Vec3u));

	return size;
}

Res<DataFormat> VulkanDevice::image_get_format(Image image) {
	if (!image) {
		return make_err<DataFormat>(Error::INVALID_HANDLE);
	}

	VulkanImage* vk_image = (VulkanImage*)image;
	return static_cast<DataFormat>(vk_image->image_format);
}

Res<uint32_t> VulkanDevice::image_get_mip_levels(Image image) {
	if (!image) {
		return make_err<uint32_t>(Error::INVALID_HANDLE);
	}

	VulkanImage* vk_image = (VulkanImage*)image;
	return vk_image->mip_levels;
}

Res<ImageUsageFlags> VulkanDevice::image_get_image_usage(Image image) {
	if (!image) {
		return make_err<uint32_t>(Error::INVALID_HANDLE);
	}

	VulkanImage* vk_image = (VulkanImage*)image;
	return vk_image->image_usage;
}

Res<void*> VulkanDevice::image_get_native_view(Image image) const {
	if (!image)
		return make_err<void*>(Error::INVALID_HANDLE);
	VulkanImage* vk_image = (VulkanImage*)image;
	return static_cast<void*>(vk_image->vk_image_view);
}

Res<void*> VulkanDevice::sampler_get_native(Sampler sampler) const {
	if (!sampler)
		return make_err<void*>(Error::INVALID_HANDLE);
	return reinterpret_cast<void*>(sampler);
}

Res<Sampler> VulkanDevice::sampler_create(const SamplerCreateInfo& info) {
	VkSamplerCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	create_info.minFilter = static_cast<VkFilter>(info.min_filter);
	create_info.magFilter = static_cast<VkFilter>(info.mag_filter);

	create_info.addressModeU = static_cast<VkSamplerAddressMode>(info.wrap_u);
	create_info.addressModeV = static_cast<VkSamplerAddressMode>(info.wrap_v);
	create_info.addressModeW = static_cast<VkSamplerAddressMode>(info.wrap_w);

	if (info.mip_levels > 0) {
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.minLod = 0.0f;
		create_info.maxLod = static_cast<float>(info.mip_levels);
		create_info.mipLodBias = 0.0f;
	}

	VkSampler vk_sampler = VK_NULL_HANDLE;
	if (vkCreateSampler(_device, &create_info, nullptr, &vk_sampler) != VK_SUCCESS) {
		return make_err<Sampler>(Error::SAMPLER_CREATION_FAILED);
	}

	return Sampler(vk_sampler);
}

Res<> VulkanDevice::sampler_free(Sampler sampler) {
	if (!sampler) {
		return {};
	}

	vkDestroySampler(_device, (VkSampler)sampler, nullptr);

	return {};
}

Res<> VulkanDevice::image_upload(Image image, const void* data, size_t size) {
	VulkanImage* vk_image = (VulkanImage*)image;
	if (!vk_image || !data || size == 0) {
		return Error::INVALID_ARGUMENT;
	}

	auto staging_res =
			buffer_create(size, BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryAllocationType::CPU);
	if (staging_res.is_error()) {
		return staging_res.error();
	}
	Buffer staging = staging_res.value();

	auto map_res = buffer_map(staging);
	if (map_res.is_error()) {
		buffer_free(staging);
		return map_res.error();
	}
	memcpy(map_res.value(), data, size);
	buffer_unmap(staging);

	const bool needs_mipmaps = vk_image->mip_levels > 1;
	const uint32_t array_layers = vk_image->array_layers;
	const size_t layer_size = vk_image->image_extent.width * vk_image->image_extent.height *
			get_data_format_size(static_cast<DataFormat>(vk_image->image_format));

	std::vector<BufferImageCopyRegion> regions;
	regions.reserve(array_layers);
	for (uint32_t layer = 0; layer < array_layers; layer++) {
		BufferImageCopyRegion region = {};
		region.buffer_offset = layer * layer_size;
		region.image_subresource.aspect_mask = IMAGE_ASPECT_COLOR_BIT;
		region.image_subresource.base_array_layer = layer;
		region.image_subresource.layer_count = 1;
		region.image_extent = {
			vk_image->image_extent.width, vk_image->image_extent.height, 1
		};
		regions.push_back(region);
	}

	Res<> submit_res = command_immediate_submit([&](CommandBuffer cmd) {
		command_transition_image(
				cmd, image, ImageLayout::UNDEFINED, ImageLayout::TRANSFER_DST_OPTIMAL);

		command_copy_buffer_to_image(cmd, staging, image, regions);

		if (needs_mipmaps) {
			command_transition_image(cmd, image, ImageLayout::TRANSFER_DST_OPTIMAL,
					ImageLayout::TRANSFER_SRC_OPTIMAL, 0, 1);
			command_generate_mipmaps(cmd, image);
		} else {
			command_transition_image(cmd, image, ImageLayout::TRANSFER_DST_OPTIMAL,
					ImageLayout::SHADER_READ_ONLY_OPTIMAL);
		}
	});

	buffer_free(staging);

	return submit_res;
}

} //namespace gpukitkit
